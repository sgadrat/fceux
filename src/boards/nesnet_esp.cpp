#include "nesnet_esp.h"
#include "../fceu.h"

#if defined(_WIN32) || defined(WIN32)

//Note: do not include UDP networking, mongoose.h should have done it correctly taking care of defining portability macros

// Compatibility hacks
typedef SSIZE_T ssize_t;
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)
#define cast_network_payload(x) reinterpret_cast<char const*>(x)

#else

#include <netdb.h>

// UDP networking
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>

// Compatibility hacks
#define cast_network_payload(x) reinterpret_cast<void const*>(x)

#endif

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

		if ((buffer[0] & 0xf0) == 0) {
			// NEXT command: looks like a MESSAGE command, but does not care about size field
			return true;
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
		UDBG("InlFirmware full command: ");
		for (uint8 const c: this->command_buffer) {
			UDBG("%02x", c);
		}
		UDBG("\n");
#endif

		// Call apropriate command handler
		switch (getCommandType(this->command_buffer[0])) {
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
	UDBG("InlFrimware TODO implement variable protocol\n");
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

	if (!message.write && !message.long_form) {
		// NEXT command
		//TODO
		UDBG("InlFrimware TODO implement NEXT command\n");
	}else if (message.connection == 0xf) {
		// Special message for ESP
		if (message.size < 1) {
			UDBG("InlFrimware received empty metadata message\n");
			return;
		}

		if ((message.payload[0] & 0xf0) == 0x10) {
			// Modify connection
			uint8 connection_number = message.payload[0] & 0x0f;
			if (connection_number > this->connections.size() - 1) {
				UDBG("InlFirmware Modify connection: invalid connection number %d\n", connection_number);
				return;
			}

			if (message.size < 3) {
				UDBG("InlFirmware Modify connection: message too short\n");
				return;
			}

			switch (message.payload[1]) {
			case 0:
				this->connections_info[connection_number].address = std::string(reinterpret_cast<char const*>(message.payload + 2), message.size - 2);
				break;
			case 1:
				if (message.size < 4) {
					UDBG("InlFirmware Modify connection: port message too short\n");
					return;
				}
				this->connections_info[connection_number].port = (static_cast<uint16>(message.payload[3]) << 8) + message.payload[2];
				break;
			case 2:
				if (message.payload[2] == 0) {
					this->connections_info[connection_number].protocol = message.payload[2];
				}else {
					UDBG("InlFirmware Modify connection: unknown protocol number %d\n", message.payload[2]);
				}
				break;
			default:
				UDBG("InlFirmware Modify connection: unknown property number %d\n", message.payload[1]);
			};

			if (this->connections_info[connection_number].isComplete()) {
				this->initConnection(connection_number);
			}
		}else {
			UDBG("InlFirmware unimplemented metadata message %02x\n", message.payload[0]);
		}
	}else {
		// Message to an actual connection
		UDBG("InlFrimware Sending message to connection #%d: ", message.connection);
		for (int i = 0; i < message.size; ++i) {
			uint8 c = message.payload[i];
			if (32 <= c && c <= 127) {
				UDBG("%c", c);
			}else {
				UDBG("\\x%02x", c);
			}
		}
		UDBG("\n");

		// Check connection status
		UdpConnection& conn = this->connections.at(message.connection);
		if (conn.fd == -1) {
			UDBG("InlFrimware unable to send message: conection is not open\n");
			return;
		}

		// Send data
		ssize_t n = sendto(
			conn.fd, cast_network_payload(message.payload), message.size, 0,
			reinterpret_cast<sockaddr*>(&conn.server_addr), sizeof(sockaddr)
		);
		if (n == -1) {
			UDBG("InlFirmware UDP send failed: %s\n", strerror(errno));
		}else if (n != message.size) {
			UDBG("InlFirmware UDP sent partial message\n");
		}
	}
}

void InlFirmware::initConnection(uint8 const connection_number) {
	ConnectionInfo const& conn_info = this->connections_info.at(connection_number);
	UdpConnection& conn = this->connections.at(connection_number);

	// Init UDP socket and store parsed address
	hostent *he = gethostbyname(conn_info.address.value().c_str());
	if (he == NULL) {
		UDBG("InlFirmware unable to resolve UDP server's hostname \"%s\"\n", conn_info.address.value());
		return;
	}
	bzero(reinterpret_cast<void*>(&conn.server_addr), sizeof(conn.server_addr));
	conn.server_addr.sin_family = AF_INET;
	conn.server_addr.sin_port = htons(conn_info.port.value());
	conn.server_addr.sin_addr = *((in_addr*)he->h_addr);

	conn.fd = ::socket(AF_INET, SOCK_DGRAM, 0);
	if (conn.fd == -1) {
		UDBG("InlFirmware unable to connect to UDP server: %s\n", strerror(errno));
	}

	sockaddr_in bind_addr;
	bzero(reinterpret_cast<void*>(&bind_addr), sizeof(bind_addr));
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_port = htons(0);
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind(conn.fd, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(sockaddr));
}

uint8 InlFirmware::tx() {
	return this->data_register;
}
