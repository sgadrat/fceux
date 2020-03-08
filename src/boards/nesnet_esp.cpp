#include "nesnet_esp.h"

#undef NESNET_DEBUG
//define NESNET_DEBUG

#ifdef NESNET_DEBUG
#define UDBG(...) FCEU_printf(__VA_ARGS__)
#else
#define UDBG(...)
#endif

namespace {
	enum class CommandType {
		VARIABLE,
		SPECIAL,
		MESSAGE,
	};

	constexpr uint8 COMMAND_TYPE_VARIABLE_FLAG = 0b01000000;
	constexpr uint8 COMMAND_TYPE_SPECIAL_FLAG = 0b00010000;

	constexpr uint8 SPECIAL_CMD_RESET = 0b0000;
	constexpr uint8 SPECIAL_CMD_MARK_READ = 0b0001;
	constexpr uint8 SPECIAL_CMD_MSG_POLL = 0b1001;
	constexpr uint8 SPECIAL_CMD_MSG_SENT = 0b1010;

	constexpr uint8 MESSAGE_CMD_LONG_MED_MASK = 0b00100000;
	constexpr uint8 MESSAGE_CMD_MED_SIZE_MASK = 0b00001111;

	CommandType getCommandType(uint8 const command_byte) {
		CommandType cmd_type = CommandType::MESSAGE;
		if (command_byte & COMMAND_TYPE_VARIABLE_FLAG) {
			cmd_type = CommandType::VARIABLE;
		}else if (command_byte & COMMAND_TYPE_SPECIAL_FLAG) {
			cmd_type = CommandType::SPECIAL;
		}
		return cmd_type;
	}

	bool commandComplete(std::vector<uint8> const& buffer) {
		if (buffer.empty()) {
			return false;
		}

		switch (getCommandType(buffer[0])) {
		case CommandType::VARIABLE:
			return buffer.size() >= 2;
		case CommandType::SPECIAL:
			return buffer.size() >= 1;
		case CommandType::MESSAGE: {
			bool medium = buffer[0] & MESSAGE_CMD_LONG_MED_MASK;
			if (medium) {
				return buffer.size() >= 1 + (buffer[0] & MESSAGE_CMD_MED_SIZE_MASK);
			}else {
				if (buffer.size() < 2) {
					return false;
				}
				return buffer.size() >= 2 + buffer[1];
			}
		}
		default:
			return false;
		};
	}
}

void InlFirmware::rx(uint8 v) {
	// Store byte in data register
	this->data_register = v;

	// Bufferise the byte and handle the command if it is complete
	this->command_buffer.push_back(v);
	if (commandComplete(this->command_buffer)) {
		// Call apropriate command handler
		switch (getCommandType(v)) {
		case CommandType::VARIABLE:
			this->cmdHandlerVariable();
			break;
		case CommandType::SPECIAL:
			this->cmdHandlerSpecial();
			break;
		case CommandType::MESSAGE:
			this->cmdHandlerMessage();
			break;
		};

		// Clear command buffer
		this->command_buffer.clear();
	}
}


void InlFirmware::cmdHandlerVariable() {
	//TODO
}

void InlFirmware::cmdHandlerSpecial() {
	const uint8 special_cmd_mask = 0b00001111;
	const uint8 cmd = this->command_buffer[0] & special_cmd_mask;

	switch (cmd) {
	case SPECIAL_CMD_RESET:
		this->data_register = 0xa5;
		break;
	default:
		UDBG("InlFrimware unknown special command %x (%02x)", cmd, this->data_register);
	};
}

void InlFirmware::cmdHandlerMessage() {
	//TODO
}

uint8 InlFirmware::tx() {
	return this->data_register;
}
