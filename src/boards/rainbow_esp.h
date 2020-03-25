#ifndef _RAINBOW_ESP_H_
#define _RAINBOW_ESP_H_

#include "../types.h"

#include "easywsclient.hpp"
#include "mongoose.h"

#include <array>
#include <atomic>
#include <deque>
#include <thread>

#include "esp.h"

//////////////////////////////////////
// BrokeStudio's ESP firmware implementation

static uint8 const NO_WORKING_FILE = 0xff;
static uint8 const NUM_FILE_PATHS = 3;
static uint8 const NUM_FILES = 64;

class BrokeStudioFirmware: public EspFirmware {
public:
	BrokeStudioFirmware();
	~BrokeStudioFirmware();

	void rx(uint8 v) override;
	uint8 tx() override;

	virtual void setGpio15(bool v) override;
	virtual bool getGpio15() override;

private:
	// Defined message types from CPU to ESP
	enum class n2e_cmds_t : uint8 {
		GET_ESP_STATUS,
		DEBUG_LOG,
		CLEAR_BUFFERS,
		GET_WIFI_STATUS,
		GET_RND_BYTE,
		GET_RND_BYTE_RANGE,
		GET_RND_WORD,
		GET_RND_WORD_RANGE,

		GET_SERVER_STATUS,
		SET_SERVER_PROTOCOL,
		GET_SERVER_SETTINGS,
		SET_SERVER_SETTINGS,
		CONNECT_TO_SERVER,
		DISCONNECT_FROM_SERVER,
		SEND_MESSAGE_TO_SERVER,

		FILE_OPEN,
		FILE_CLOSE,
		FILE_EXISTS,
		FILE_DELETE,
		FILE_SET_CUR,
		FILE_READ,
		FILE_WRITE,
		FILE_APPEND,
		FILE_COUNT,
		FILE_GET_LIST,
		FILE_GET_FREE_ID,
		FILE_GET_INFO,
	};

	// Defined message types from ESP to CPU
	enum class e2n_cmds_t : uint8 {
		READY,

		FILE_EXISTS,
		FILE_DELETE,
		FILE_LIST,
		FILE_DATA,
		FILE_COUNT,
		FILE_ID,
		FILE_INFO,

		WIFI_STATUS,
		SERVER_STATUS,
		HOST_SETTINGS,

		RND_BYTE,
		RND_WORD,

		MESSAGE_FROM_SERVER,
	};

	enum class server_protocol_t : uint8 {
		WEBSOCKET,
		UDP,
	};

	void processBufferedMessage();
	void readFile(uint8 path, uint8 file, uint8 n, uint32 offset);
	template<class I>
	void writeFile(uint8 path, uint8 file, uint32 offset, I data_begin, I data_end);
	uint8 getFreeFileId(uint8 path) const;

	template<class I>
	void sendMessageToServer(I begin, I end);
	template<class I>
	void sendUdpDatagramToServer(I begin, I end);
	void receiveDataFromServer();

	void closeConnection();
	void openConnection();

	static std::string pathStrFromIndex(uint8 path, uint8 file);
	static std::pair<uint8, uint8> pathIndexFromStr(std::string const&);

	static void httpdEvent(mg_connection *nc, int ev, void *ev_data);

private:
	std::deque<uint8> rx_buffer;
	std::deque<uint8> tx_buffer;

	std::array<std::array<std::vector<uint8>, NUM_FILES>, NUM_FILE_PATHS> files;
	std::array<std::array<bool, NUM_FILES>, NUM_FILE_PATHS> file_exists;
	uint32 file_offset = 0;
	uint8 working_path = 0;
	uint8 working_file = NO_WORKING_FILE;

	server_protocol_t active_protocol = server_protocol_t::WEBSOCKET;
	std::string server_settings_address;
	uint16_t server_settings_port = 0;

	easywsclient::WebSocket::pointer socket = nullptr;
	std::thread socket_close_thread;

	int udp_socket = -1;
	sockaddr_in server_addr;

	mg_mgr mgr;
	mg_connection *nc = nullptr;
	std::atomic<bool> httpd_run;
	std::thread httpd_thread;

	bool msg_first_byte = true;
	uint8 msg_length = 0;
	uint8 last_byte_read = 0;
};

#endif
