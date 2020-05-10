#ifndef _NESNET_ESP_H_
#define _NESNET_ESP_H_

#include "../types.h"
#include "esp.h"

//TODO simplify: here we are includeing a web server only for its macros standardizing winsocks
#include "mongoose.h"

#include <array>
#include <deque>
#include <vector>
#include <string>

class InlFirmware : public EspFirmware {
public:
	~InlFirmware() = default;

	void rx(uint8 v) override;
	uint8 tx() override;

	void setGpio4(bool /*v*/) override {}
	bool getGpio4() override { return false; }

private:
	void cmdHandlerVariable();
	void cmdHandlerSpecial();
	void cmdHandlerMessage();

	void initConnection(uint8 const connection_number);
	void receiveDataFromServer();
	void receivedNetworkMessage(std::vector<uint8> const& msg);

	void sendMessage(std::vector<uint8> const& payload, uint8 connexion_number);

	static constexpr size_t NUM_VARIABLES = 64;

	uint8 data_register = 0;
	std::vector<uint8> command_buffer;
	std::deque<std::vector<uint8>> message_buffers;
	std::vector<uint8>::const_iterator read_pointer;
	std::array<uint8, NUM_VARIABLES> incoming_variables; //TODO we may want to zero-initialize this array

	struct ConnectionInfo {
		// Cheap replacement for std::optional
		template <typename T>
		class my_optional {
		public:
			my_optional& operator=(T const& other) {
				this->val = other;
				this->init = true;
				return *this;
			}
			bool has_value() const { return this->init; }
			T const& value() const {
				if (!this->has_value()) {
					throw std::runtime_error("invalid access to empty optional");
				}
				return val;
			}
		private:
			T val;
			bool init = false;
		};

		my_optional<std::string> address;
		my_optional<uint16> port;
		my_optional<uint8> protocol;
		bool isComplete() {
			return this->address.has_value() && this->port.has_value() && this->protocol.has_value();
		}
	};
	struct UdpConnection {
		int fd = -1;
		sockaddr_in server_addr;
	};
	std::array<ConnectionInfo, 8> connections_info;
	std::array<UdpConnection, 8> connections;
};

#endif
