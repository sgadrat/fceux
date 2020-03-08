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
}

void InlFirmware::rx(uint8 v) {
	// Store command in data register
	this->data_register = v;

	// Decode command type
	CommandType cmd_type = CommandType::MESSAGE;
	if (v & COMMAND_TYPE_VARIABLE_FLAG) {
		cmd_type = CommandType::VARIABLE;
	}else if (v & COMMAND_TYPE_SPECIAL_FLAG) {
		cmd_type = CommandType::SPECIAL;
	}

	// Call adequate command handler
	switch (cmd_type) {
	case CommandType::VARIABLE:
		this->cmdHandlerVariable(v);
		break;
	case CommandType::SPECIAL:
		this->cmdHandlerSpecial(v);
		break;
	case CommandType::MESSAGE:
		this->cmdHandlerMessage(v);
		break;
	};
}


void InlFirmware::cmdHandlerVariable(uint8 cmd) {
	//TODO
	(void)cmd;
}

void InlFirmware::cmdHandlerSpecial(uint8 cmd) {
	const uint8 special_cmd_mask = 0b00001111;
	cmd = cmd & special_cmd_mask;

	switch (cmd) {
	case SPECIAL_CMD_RESET:
		this->data_register = 0xa5;
		break;
	default:
		UDBG("InlFrimware unknown special command %x (%02x)", cmd, this->data_register);
	};
}

void InlFirmware::cmdHandlerMessage(uint8 cmd) {
	//TODO
	(void)cmd;
}

uint8 InlFirmware::tx() {
	return this->data_register;
}
