#include "nesnet_esp.h"
#include "../fceu.h"

#undef NESNET_DEBUG
#define NESNET_DEBUG

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

	constexpr uint8 COMMAND_WRITE_FLAG = 0b10000000;
	constexpr uint8 COMMAND_TYPE_VARIABLE_FLAG = 0b01000000;
	constexpr uint8 COMMAND_TYPE_SPECIAL_FLAG = 0b00010000;

	constexpr uint8 SPECIAL_CMD_RESET = 0b0000;
	constexpr uint8 SPECIAL_CMD_MARK_READ = 0b0001;
	constexpr uint8 SPECIAL_CMD_MSG_POLL = 0b1001;
	constexpr uint8 SPECIAL_CMD_MSG_SENT = 0b1010;

	constexpr uint8 MESSAGE_CMD_LONG_MED_MASK = 0b00100000;
	constexpr uint8 MESSAGE_CMD_MED_SIZE_MASK = 0b00001111;
	constexpr uint8 MESSAGE_CMD_LONG_CONNECTION_MASK = 0b00001111;

	struct ParsedMessageCommand {
		uint8 const* payload;
		bool write;
		bool long_form;
		uint8 connection;
		uint8 size;
	};

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
			bool long_form = buffer[0] & MESSAGE_CMD_LONG_MED_MASK;
			if (long_form) {
				if (buffer.size() < 2) {
					return false;
				}
				return buffer.size() >= 2 + buffer[1];
			}else {
				return buffer.size() >= 1 + (buffer[0] & MESSAGE_CMD_MED_SIZE_MASK);
			}
		}
		default:
			return false;
		};
	}

	ParsedMessageCommand parseMessageCommand(std::vector<uint8> const& command) {
		ParsedMessageCommand parsed;
		parsed.write = command[0] & COMMAND_WRITE_FLAG;
		parsed.long_form = command[0] & MESSAGE_CMD_LONG_MED_MASK;

		if (parsed.long_form) {
			parsed.connection = command[0] & MESSAGE_CMD_LONG_CONNECTION_MASK;
			parsed.size = command[1];
			parsed.payload = command.data() + 2;
		}else {
			parsed.connection = 0;
			parsed.size = command[0] & MESSAGE_CMD_MED_SIZE_MASK;
			parsed.payload = command.data() + 1;
		}

		return parsed;
	}
}

void InlFirmware::rx(uint8 v) {
	// Store byte in data register
	this->data_register = v;

	// Bufferise the byte and handle the command if it is complete
	this->command_buffer.push_back(v);
	if (commandComplete(this->command_buffer)) {
#if 0
		UDBG("full command: ");
		for (uint8 const c: this->command_buffer) {
			UDBG("%02x", c);
		}
		UDBG("\n");
#endif

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
	UDBG("TODO implement variable protocol\n");
}

void InlFirmware::cmdHandlerSpecial() {
	const uint8 special_cmd_mask = 0b00001111;
	const uint8 cmd = this->command_buffer[0] & special_cmd_mask;

	switch (cmd) {
	case SPECIAL_CMD_RESET:
		this->data_register = 0xa5;
		break;
	case SPECIAL_CMD_MSG_SENT:
		this->data_register = 0x00;
		break;
	default:
		UDBG("InlFrimware unknown special command %x (%02x)\n", cmd, this->data_register);
	};
}

void InlFirmware::cmdHandlerMessage() {
	ParsedMessageCommand message = parseMessageCommand(this->command_buffer);

	if (message.size == 0 && !message.write) {
		// NEXT command
		//TODO
		UDBG("TODO implement NEXT command\n");
	}else if (message.connection == 0xf) {
		// Special message for ESP
		//TODO
		UDBG("TODO implement metadata messages\n");
	}else {
		// Message to an actual connection
		UDBG("Sending message to connection #%d: ", message.connection);
		for (int i = 0; i < message.size; ++i) {
			uint8 c = message.payload[i];
			if (32 <= c && c <= 127) {
				UDBG("%c", c);
			}else {
				UDBG("\\x%02x", c);
			}
		}
		UDBG("\n");

		//TODO implement
	}
}

uint8 InlFirmware::tx() {
	return this->data_register;
}
