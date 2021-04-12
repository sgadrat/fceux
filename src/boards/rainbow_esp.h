#ifndef _RAINBOW_ESP_H_
#define _RAINBOW_ESP_H_

#include "../types.h"

#include "easywsclient.hpp"
#include "mongoose.h"

#include <curl/curl.h>

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
static uint8 const NUM_NETWORKS = 3;
static uint8 const NUM_FAKE_NETWORKS = 5;
static uint8 const SSID_MAX_LENGTH = 32;
static uint8 const PASS_MAX_LENGTH = 64;

struct NetworkInfo
{
	std::string ssid;
	std::string pass;
};

class BrokeStudioFirmware: public EspFirmware {
public:
	BrokeStudioFirmware();
	~BrokeStudioFirmware();

	void rx(uint8 v) override;
	uint8 tx() override;

	virtual void setGpio4(bool v) override;
	virtual bool getGpio4() override;

private:
	// Defined message types from CPU to ESP
	enum class toesp_cmds_t : uint8 {
		// ESP CMDS
		ESP_GET_STATUS,
		DEBUG_GET_LEVEL,
		DEBUG_SET_LEVEL,
		DEBUG_LOG,
		BUFFER_CLEAR_RX_TX,
		BUFFER_DROP_FROM_ESP,
		WIFI_GET_STATUS,
		ESP_RESTART,

		// RND CMDS
		RND_GET_BYTE,
		RND_GET_BYTE_RANGE, // ; min / max
		RND_GET_WORD,
		RND_GET_WORD_RANGE, // ; min / max

		// SERVER CMDS
		SERVER_GET_STATUS,
		SERVER_PING,
		SERVER_SET_PROTOCOL,
		SERVER_GET_SETTINGS,
		SERVER_GET_CONFIG_SETTINGS,
		SERVER_SET_SETTINGS,
		SERVER_RESTORE_SETTINGS,
		SERVER_CONNECT,
		SERVER_DISCONNECT,
		SERVER_SEND_MSG,

		// NETWORK CMDS
		NETWORK_SCAN,
		NETWORK_GET_DETAILS,
		NETWORK_GET_REGISTERED,
		NETWORK_GET_REGISTERED_DETAILS,
		NETWORK_REGISTER,
		NETWORK_UNREGISTER,

		// FILE CMDS
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
		FILE_DOWNLOAD,
	};

	// Defined message types from ESP to CPU
	enum class fromesp_cmds_t : uint8 {
		// ESP CMDS
		READY,
		DEBUG_LEVEL,
		WIFI_STATUS,

		// RND CMDS
		RND_BYTE,
		RND_WORD,

		// SERVER CMDS
		SERVER_STATUS,
		SERVER_PING,
		SERVER_SETTINGS,
		MESSAGE_FROM_SERVER,

		// NETWORK CMDS
		NETWORK_COUNT,
		NETWORK_SCANNED_DETAILS,
		NETWORK_REGISTERED_DETAILS,
		NETWORK_REGISTERED,

		// FILE CMDS
		FILE_EXISTS,
		FILE_DELETE,
		FILE_LIST,
		FILE_DATA,
		FILE_COUNT,
		FILE_ID,
		FILE_INFO,
		FILE_DOWNLOAD,
	};

	enum class server_protocol_t : uint8 {
		WEBSOCKET,
		WEBSOCKET_SECURED,
		TCP,
		TCP_SECURED,
		UDP,
	};

	enum class file_delete_results_t : uint8
	{
		SUCCESS,
		ERROR_WHILE_DELETING_FILE,
		FILE_NOT_FOUND,
		INVALID_PATH_OR_FILE,
	};

	enum class file_download_results_t : uint8
	{
		SUCCESS,
		ERROR_WHILE_DELETING_FILE,
		DOWNLOAD_FAILED,
		INVALID_PATH_OR_FILE,
	};


	void processBufferedMessage();
	void readFile(uint8 path, uint8 file, uint8 n, uint32 offset);
	template<class I>
	void writeFile(uint8 path, uint8 file, uint32 offset, I data_begin, I data_end);
	uint8 getFreeFileId(uint8 path) const;
	void saveFiles() const;
	void loadFiles();

	template<class I>
	void sendMessageToServer(I begin, I end);
	template<class I>
	void sendUdpDatagramToServer(I begin, I end);
	template<class I>
	void sendTcpDataToServer(I begin, I end);
	void receiveDataFromServer();

	void closeConnection();
	void openConnection();

	void pingRequest(uint8 n);
	void receivePingResult();

	std::pair<bool, sockaddr_in> resolve_server_address();
	static std::deque<uint8> read_socket(int socket);

	static void httpdEvent(mg_connection *nc, int ev, void *ev_data);

	void initDownload();
	void downloadFile(std::string const& url, uint8_t path, uint8_t file);
	void cleanupDownload();

private:
	std::deque<uint8> rx_buffer;
	std::deque<uint8> tx_buffer;
	std::deque<std::deque<uint8>> tx_messages;

	std::array<std::array<std::vector<uint8>, NUM_FILES>, NUM_FILE_PATHS> files;
	std::array<std::array<bool, NUM_FILES>, NUM_FILE_PATHS> file_exists;
	uint32 file_offset = 0;
	uint8 working_path = 0;
	uint8 working_file = NO_WORKING_FILE;

	std::array<NetworkInfo, NUM_NETWORKS> networks;

	server_protocol_t active_protocol = server_protocol_t::WEBSOCKET;
	std::string default_server_settings_address;
	uint16_t default_server_settings_port = 0;
	std::string server_settings_address;
	uint16_t server_settings_port = 0;

	uint8 debug_config = 0;

	easywsclient::WebSocket::pointer socket = nullptr;
	std::thread socket_close_thread;

	uint8 ping_min = 0;
	uint8 ping_avg = 0;
	uint8 ping_max = 0;
	uint8 ping_lost = 0;
	std::atomic<bool> ping_ready;
	std::thread ping_thread;

	int udp_socket = -1;
	sockaddr_in server_addr;

	int tcp_socket = -1;

	mg_mgr mgr;
	mg_connection *nc = nullptr;
	std::atomic<bool> httpd_run;
	std::thread httpd_thread;

	bool msg_first_byte = true;
	uint8 msg_length = 0;
	uint8 last_byte_read = 0;

	CURL* curl_handle = nullptr;
};

#endif
