#include "rainbow_esp.h"

#include "../fceu.h"
#include "../utils/crc32.h"

#include "pping.h"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <map>
#include <sstream>

#if defined(_WIN32) || defined(WIN32)

//Note: do not include UDP networking, mongoose.h should have done it correctly taking care of defining portability macros

// Compatibility hacks
typedef SSIZE_T ssize_t;
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)
#define cast_network_payload(x) reinterpret_cast<char*>(x)

#else

// UDP networking
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/udp.h>

// Compatibility hacks
#define cast_network_payload(x) reinterpret_cast<void*>(x)

#endif

using easywsclient::WebSocket;

#define RAINBOW_DEBUG 1

#if RAINBOW_DEBUG >= 1
#define UDBG(...) FCEU_printf(__VA_ARGS__)
#else
#define UDBG(...)
#endif

#if RAINBOW_DEBUG >= 2
#define UDBG_FLOOD(...) FCEU_printf(__VA_ARGS__)
#else
#define UDBG_FLOOD(...)
#endif

namespace {

std::vector<uint8> readHostFile(std::string const& file_path) {
	// Open file
	std::ifstream ifs(file_path, std::ifstream::binary);
	if (!ifs) {
		throw std::runtime_error("unable to open file");
	}

	// Get file length
	ifs.seekg(0, ifs.end);
	size_t const file_length = ifs.tellg();
	if (ifs.fail()) {
		throw std::runtime_error("unable to get file length");
	}
	ifs.seekg(0, ifs.beg);

	// Read file
	std::vector<uint8> data(file_length);
	ifs.read(reinterpret_cast<char*>(data.data()), file_length);
	if (ifs.fail()) {
		throw std::runtime_error("error while reading file");
	}

	return data;
}

std::array<std::string, NUM_FILE_PATHS> dir_names = { "SAVE", "ROMS", "USER" };

}

BrokeStudioFirmware::BrokeStudioFirmware() {
	UDBG("RAINBOW BrokeStudioFirmware ctor\n");

	// Get default host/port
	char const* hostname = ::getenv("RAINBOW_SERVER_ADDR");
	if (hostname == nullptr) hostname = "";
	this->server_settings_address = hostname;

	char const* port_cstr = ::getenv("RAINBOW_SERVER_PORT");
	if (port_cstr == nullptr) port_cstr = "3000";
	std::istringstream port_iss(port_cstr);
	port_iss >> this->server_settings_port;

	// Start web server
	this->httpd_run = true;
	char const * const httpd_port = "8080";
	mg_mgr_init(&this->mgr, reinterpret_cast<void*>(this));
	UDBG("Starting web server on port %s\n", httpd_port);
	this->nc = mg_bind(&this->mgr, httpd_port, BrokeStudioFirmware::httpdEvent);
	if (this->nc == NULL) {
		printf("Failed to create web server\n");
	}else {
		mg_set_protocol_http_websocket(this->nc);
		this->httpd_thread = std::thread([this] {
			while (this->httpd_run) {
				mg_mgr_poll(&this->mgr, 1000);
			}
			mg_mgr_free(&this->mgr);
		});
	}

	// Clear file list
	for (uint8 p = 0; p < NUM_FILE_PATHS; p++)
	{
		for (uint8 f = 0; f < NUM_FILES; f++)
		{
			this->file_exists[p][f] = false;
		}
	}

	// Load file list from save file (if any)
	this->loadFiles();

	// Mark ping result as useless
	this->ping_ready = false;
}

BrokeStudioFirmware::~BrokeStudioFirmware() {
	UDBG("RAINBOW BrokeStudioFirmware dtor\n");
	if (this->socket != nullptr) {
		delete this->socket;
		this->socket = nullptr;
	}

	this->httpd_run = false;
	if (this->httpd_thread.joinable()) {
		this->httpd_thread.join();
	}
}

void BrokeStudioFirmware::rx(uint8 v) {
	UDBG_FLOOD("RAINBOW BrokeStudioFirmware rx %02x\n", v);
	if (this->msg_first_byte) {
		this->msg_first_byte = false;
		this->msg_length = v + 1;
	}
	this->rx_buffer.push_back(v);

	if (this->rx_buffer.size() == this->msg_length) {
		this->processBufferedMessage();
		this->msg_first_byte = true;
	}
}

uint8 BrokeStudioFirmware::tx() {
	UDBG_FLOOD("RAINBOW BrokeStudioFirmware tx\n");

	// Refresh buffer from network
	this->receiveDataFromServer();
	this->receivePingResult();

	// Fill buffer with the next message (if needed)
	if (this->tx_buffer.empty() && !this->tx_messages.empty()) {
		std::deque<uint8> message = this->tx_messages.front();
		this->tx_buffer.push_back(last_byte_read);
		this->tx_buffer.insert(this->tx_buffer.end(), message.begin(), message.end());
		this->tx_messages.pop_front();
	}

	// Get byte from buffer
	if (!this->tx_buffer.empty()) {
		last_byte_read = this->tx_buffer.front();
		this->tx_buffer.pop_front();
	}

	//UDBG("RAINBOW BrokeStudioFirmware tx <= %02x\n", last_byte_read);
	return last_byte_read;
}

void BrokeStudioFirmware::setGpio4(bool /*v*/) {
}

bool BrokeStudioFirmware::getGpio4() {
	this->receiveDataFromServer();
	this->receivePingResult();
	return !(this->tx_buffer.empty() && this->tx_messages.empty());
}

void BrokeStudioFirmware::processBufferedMessage() {
	assert(this->rx_buffer.size() >= 2); // Buffer must contain exactly one message, minimal message is two bytes (length + type)
	uint8 const message_size = this->rx_buffer.front();
	assert(message_size >= 1); // minimal payload is one byte (type)
	assert(this->rx_buffer.size() == static_cast<std::deque<uint8>::size_type>(message_size) + 1); // Buffer size must match declared payload size

	// Process the message in RX buffer
	switch (static_cast<n2e_cmds_t>(this->rx_buffer.at(1))) {
		case n2e_cmds_t::GET_ESP_STATUS:
			UDBG("RAINBOW BrokeStudioFirmware received message GET_ESP_STATUS\n");
			this->tx_messages.push_back({1, static_cast<uint8>(e2n_cmds_t::READY)});
			break;
		case n2e_cmds_t::DEBUG_LOG:
			#if RAINBOW_DEBUG >= 1
				FCEU_printf("RAINBOW DEBUG/LOG: ");
				for (std::deque<uint8>::const_iterator cur = this->rx_buffer.begin() + 2; cur < this->rx_buffer.end(); ++cur) {
					FCEU_printf("%02x ", *cur);
				}
				FCEU_printf("\n");
			#endif
			break;
		case n2e_cmds_t::CLEAR_BUFFERS:
			UDBG("RAINBOW BrokeStudioFirmware received message CLEAR_BUFFERS\n");
			this->receiveDataFromServer();
			this->receivePingResult();
			this->tx_buffer.clear();
			this->tx_messages.clear();
			this->rx_buffer.clear();
			break;
		case n2e_cmds_t::E2N_BUFFER_DROP:
			UDBG("RAINBOW BrokeStudioFirmware received message E2N_BUFFER_DROP\n");
			if (message_size == 3) {
				uint8 const message_type = this->rx_buffer.at(2);
				uint8 const n_keep = this->rx_buffer.at(3);

				size_t i = 0;
				for (
					std::deque<std::deque<uint8>>::iterator message = this->tx_messages.end();
					message != this->tx_messages.begin();
				)
				{
					--message;
					if (message->at(1) == message_type) {
						++i;
						if (i > n_keep) {
							UDBG("RAINBOW BrokeStudioFirmware erase message: index=%d\n", message - this->tx_messages.begin());
							message = this->tx_messages.erase(message);
						}else {
							UDBG("RAINBOW BrokeStudioFirmware keep message: index=%d - too recent\n", message - this->tx_messages.begin());
						}
					}else {
						UDBG("RAINBOW BrokeStudioFirmware keep message: index=%d - bad type\n", message - this->tx_messages.begin());
					}
				}
			}
			break;
		case n2e_cmds_t::GET_WIFI_STATUS:
			UDBG("RAINBOW BrokeStudioFirmware received message GET_WIFI_STATUS\n");
			this->tx_messages.push_back({2, static_cast<uint8>(e2n_cmds_t::WIFI_STATUS), 3}); // Simple answer, wifi is ok
			break;
		case n2e_cmds_t::GET_RND_BYTE:
			UDBG("RAINBOW BrokeStudioFirmware received message GET_RND_BYTE\n");
			this->tx_messages.push_back({
				2,
				static_cast<uint8>(e2n_cmds_t::RND_BYTE),
				static_cast<uint8>(rand() % 256)
			});
			break;
		case n2e_cmds_t::GET_RND_BYTE_RANGE: {
			UDBG("RAINBOW BrokeStudioFirmware received message GET_RND_BYTE_RANGE\n");
			int const min_value = this->rx_buffer.at(2);
			int const max_value = this->rx_buffer.at(3);
			int const range = max_value - min_value;
			this->tx_messages.push_back({
				2,
				static_cast<uint8>(e2n_cmds_t::RND_BYTE),
				static_cast<uint8>(min_value + (rand() % range))
			});
			break;
		}
		case n2e_cmds_t::GET_RND_WORD:
			UDBG("RAINBOW BrokeStudioFirmware received message GET_RND_WORD\n");
			this->tx_messages.push_back({
				3,
				static_cast<uint8>(e2n_cmds_t::RND_WORD),
				static_cast<uint8>(rand() % 256),
				static_cast<uint8>(rand() % 256)
			});
			break;
		case n2e_cmds_t::GET_RND_WORD_RANGE: {
			UDBG("RAINBOW BrokeStudioFirmware received message GET_RND_WORD_RANGE\n");
			int const min_value = (static_cast<int>(this->rx_buffer.at(2)) << 8) + this->rx_buffer.at(3);
			int const max_value = (static_cast<int>(this->rx_buffer.at(4)) << 8) + this->rx_buffer.at(5);
			int const range = max_value - min_value;
			int const rand_value = min_value + (rand() % range);
			this->tx_messages.push_back({
				3,
				static_cast<uint8>(e2n_cmds_t::RND_WORD),
				static_cast<uint8>(rand_value >> 8),
				static_cast<uint8>(rand_value & 0xff)
			});
			break;
		}
		case n2e_cmds_t::GET_SERVER_STATUS: {
			UDBG("RAINBOW BrokeStudioFirmware received message GET_SERVER_STATUS\n");
			uint8 status;
			switch (this->active_protocol) {
			case server_protocol_t::WEBSOCKET:
				status = (this->socket != nullptr); // Server connection is ok if we succeed to open it
				break;
			case server_protocol_t::UDP:
				status = (this->udp_socket != -1); // Considere server connection ok if we created a socket
				break;
			default:
				status = 0; // Unknown active protocol, connection certainly broken
			}

			this->tx_messages.push_back({
				2,
				static_cast<uint8>(e2n_cmds_t::SERVER_STATUS),
				status
			});
			break;
		}
		case n2e_cmds_t::GET_SERVER_PING:
			UDBG("RAINBOW BrokeStudioFirmware received message GET_SERVER_PING\n");
			if (!this->ping_thread.joinable()) {
				if (this->server_settings_address.empty()) {
					this->tx_messages.push_back({
						1,
						static_cast<uint8>(e2n_cmds_t::SERVER_PING)
					});
				}else if (message_size >= 1) {
					assert(!this->ping_thread.joinable());
					this->ping_ready = false;
					uint8 n = (message_size == 1 ? 0 : this->rx_buffer.at(2));
					if (n == 0) {
						n = 4;
					}
					this->ping_thread = std::thread(&BrokeStudioFirmware::pingRequest, this, n);
				}
			}
			break;
		case n2e_cmds_t::SET_SERVER_PROTOCOL: {
			UDBG("RAINBOW BrokeStudioFirmware received message SET_SERVER_PROTOCOL\n");
			if (message_size == 2) {
				server_protocol_t const requested_protocol = static_cast<server_protocol_t>(this->rx_buffer.at(2));
				if (requested_protocol > server_protocol_t::UDP) {
					UDBG("RAINBOW BrokeStudioFirmware SET_SERVER_PROTOCOL: unknown protocol (%d)\n", requested_protocol);
				}else {
					this->active_protocol = requested_protocol;
				}
			}
			break;
		}
		case n2e_cmds_t::GET_SERVER_SETTINGS: {
			UDBG("RAINBOW BrokeStudioFirmware received message GET_SERVER_SETTINGS\n");
			if (this->server_settings_address.empty()) {
				this->tx_messages.push_back({
					1,
					static_cast<uint8>(e2n_cmds_t::HOST_SETTINGS)
				});
			}else {
				std::deque<uint8> message({
					static_cast<uint8>(1 + 2 + this->server_settings_address.size()),
					static_cast<uint8>(e2n_cmds_t::HOST_SETTINGS),
					static_cast<uint8>(this->server_settings_port >> 8),
					static_cast<uint8>(this->server_settings_port & 0xff)
				});
				message.insert(message.end(), this->server_settings_address.begin(), this->server_settings_address.end());
				this->tx_messages.push_back(message);
			}
			break;
		}
		case n2e_cmds_t::SET_SERVER_SETTINGS:
			UDBG("RAINBOW BrokeStudioFirmware received message SET_SERVER_SETTINGS\n");
			if (message_size >= 3) {
				this->server_settings_port =
					(static_cast<uint16_t>(this->rx_buffer.at(2)) << 8) +
					(static_cast<uint16_t>(this->rx_buffer.at(3)))
				;
				this->server_settings_address = std::string(this->rx_buffer.begin()+4, this->rx_buffer.end());
			}
			break;
		case n2e_cmds_t::CONNECT_TO_SERVER:
			UDBG("RAINBOW BrokeStudioFirmware received message CONNECT_TO_SERVER\n");
			this->openConnection();
			break;
		case n2e_cmds_t::DISCONNECT_FROM_SERVER:
			UDBG("RAINBOW BrokeStudioFirmware received message DISCONNECT_FROM_SERVER\n");
			this->closeConnection();
			break;
		case n2e_cmds_t::SEND_MESSAGE_TO_SERVER: {
			UDBG("RAINBOW BrokeStudioFirmware received message SEND_MESSAGE\n");
			uint8 const payload_size = this->rx_buffer.size() - 2;
			std::deque<uint8>::const_iterator payload_begin = this->rx_buffer.begin() + 2;
			std::deque<uint8>::const_iterator payload_end = payload_begin + payload_size;

			switch (this->active_protocol) {
			case server_protocol_t::WEBSOCKET:
				this->sendMessageToServer(payload_begin, payload_end);
				break;
			case server_protocol_t::UDP:
				this->sendUdpDatagramToServer(payload_begin, payload_end);
				break;
			default:
				UDBG("RAINBOW BrokeStudioFirmware active protocol (%d) not implemented\n", this->active_protocol);
			};
			break;
		}
		case n2e_cmds_t::FILE_OPEN:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_OPEN\n");
			if (message_size == 3) {
				uint8 const path = this->rx_buffer.at(2);
				uint8 const file = this->rx_buffer.at(3);
				if (path < NUM_FILE_PATHS && file < NUM_FILES) {
					this->file_exists[path][file] = true;
					this->working_path = path;
					this->working_file = file;
					this->file_offset = 0;
					this->saveFiles();
				}
			}
			break;
		case n2e_cmds_t::FILE_CLOSE:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_CLOSE\n");
			this->working_file = NO_WORKING_FILE;
			this->saveFiles();
			break;
		case n2e_cmds_t::FILE_EXISTS:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_EXISTS\n");
			if (message_size == 3) {
				uint8 const path = this->rx_buffer.at(2);
				uint8 const file = this->rx_buffer.at(3);
				if (path < NUM_FILE_PATHS && file < NUM_FILES) {
					this->tx_messages.push_back({
						2,
						static_cast<uint8>(e2n_cmds_t::FILE_EXISTS),
						static_cast<uint8>(this->file_exists[path][file] ? 1 : 0)
					});
				}
			}
			break;
		case n2e_cmds_t::FILE_DELETE:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_DELETE\n");
			if (message_size == 3) {
				uint8 const path = this->rx_buffer.at(2);
				uint8 const file = this->rx_buffer.at(3);
				if (path < NUM_FILE_PATHS && file < NUM_FILES) {
					if (this->file_exists[path][file]) {
						// File exists, let's delete it
						this->files[path][file].clear();
						this->file_exists[path][file] = false;
						this->tx_messages.push_back({
							2,
							static_cast<uint8>(e2n_cmds_t::FILE_DELETE),
							0
						});
						this->saveFiles();
					}else {
						// File does not exist
						this->tx_messages.push_back({
							2,
							static_cast<uint8>(e2n_cmds_t::FILE_DELETE),
							2
						});
					}
				}else {
					// Error while deleting the file
					this->tx_messages.push_back({
						2,
						static_cast<uint8>(e2n_cmds_t::FILE_DELETE),
						1
					});
				}
			}
			break;
		case n2e_cmds_t::FILE_SET_CUR:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_SET_CUR\n");
			if (2 <= message_size && message_size <= 5) {
				this->file_offset = this->rx_buffer[2];
				this->file_offset += static_cast<uint32>(message_size >= 3 ? this->rx_buffer[3] : 0) << 8;
				this->file_offset += static_cast<uint32>(message_size >= 4 ? this->rx_buffer[4] : 0) << 16;
				this->file_offset += static_cast<uint32>(message_size >= 5 ? this->rx_buffer[5] : 0) << 24;
			}
			break;
		case n2e_cmds_t::FILE_READ:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_READ\n");
			if (message_size == 2) {
				if (this->working_file != NO_WORKING_FILE) {
					uint8 const n = this->rx_buffer[2];
					this->readFile(this->working_path, this->working_file, n, this->file_offset);
					this->file_offset += n;
					UDBG("working file offset: %u (%x)\n", this->file_offset, this->file_offset);
					UDBG("file size: %lu bytes\n", this->files[this->working_path][this->working_file].size());
					if (this->file_offset > this->files[this->working_path][this->working_file].size()) {
						this->file_offset = this->files[this->working_path][this->working_file].size();
					}
				}else {
					this->tx_messages.push_back({2, static_cast<uint8>(e2n_cmds_t::FILE_DATA), 0});
				}
			}
			break;
		case n2e_cmds_t::FILE_WRITE:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_WRITE\n");
			if (message_size >= 3 && this->working_file != NO_WORKING_FILE) {
				this->writeFile(this->working_path, this->working_file, this->file_offset, this->rx_buffer.begin() + 2, this->rx_buffer.begin() + message_size + 1);
				this->file_offset += message_size - 1;
			}
			break;
		case n2e_cmds_t::FILE_APPEND:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_APPEND\n");
			if (message_size >= 3 && this->working_file != NO_WORKING_FILE) {
				this->writeFile(this->working_path, this->working_file, this->files[working_path][working_file].size(), this->rx_buffer.begin() + 2, this->rx_buffer.begin() + message_size + 1);
			}
			break;
		case n2e_cmds_t::FILE_COUNT:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_COUNT\n");
			if (message_size == 2) {
				uint8 const path = this->rx_buffer[2];
				if (path >= NUM_FILE_PATHS) {
					this->tx_messages.push_back({
						2,
						static_cast<uint8>(e2n_cmds_t::FILE_COUNT),
						0
					});
				}else {
					uint8 nb_files = 0;
					for (bool exists : this->file_exists[path]) {
						if (exists) {
							++nb_files;
						}
					}
					this->tx_messages.push_back({
						2,
						static_cast<uint8>(e2n_cmds_t::FILE_COUNT),
						nb_files
					});
					UDBG("%u files found in path %u\n", nb_files, path);
				}
			}
			break;
		case n2e_cmds_t::FILE_GET_LIST:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_GET_LIST\n");
			if (message_size >= 2) {
				std::vector<uint8> existing_files;
				uint8 const path = this->rx_buffer[2];
				uint8 page_size = NUM_FILES;
				uint8 current_page = 0;
				if (message_size == 4) {
					page_size = this->rx_buffer[3];
					current_page = this->rx_buffer[4];
				}
				uint8 page_start = current_page * page_size;
				uint8 page_end = current_page * page_size + page_size;
				uint8 nFiles = 0;
				if (page_end > this->file_exists[path].size())
					page_end = this->file_exists[path].size();
				for (uint8 i = 0; i < NUM_FILES; ++i) {
					if (this->file_exists[path][i]) {
						if (nFiles >= page_start && nFiles < page_end) {
							existing_files.push_back(i);
						}
						nFiles++;
					}
					if (nFiles >= page_end) break;
				}
				std::deque<uint8> message({
					static_cast<uint8>(existing_files.size() + 2),
					static_cast<uint8>(e2n_cmds_t::FILE_LIST),
					static_cast<uint8>(existing_files.size())
				});
				message.insert(message.end(), existing_files.begin(), existing_files.end());
				this->tx_messages.push_back(message);
			}
			break;
		case n2e_cmds_t::FILE_GET_FREE_ID:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_GET_FREE_ID\n");
			if (message_size == 2) {
				uint8 const file_id = this->getFreeFileId(this->rx_buffer.at(2));
				if (file_id != 128) {
					// Free file ID found
					this->tx_messages.push_back({
						2,
						static_cast<uint8>(e2n_cmds_t::FILE_ID),
						file_id,
					});
				}else {
					// Free file ID not found
					this->tx_messages.push_back({
						1,
						static_cast<uint8>(e2n_cmds_t::FILE_ID)
					});
				}
			}
			break;
		case n2e_cmds_t::FILE_GET_INFO:
			UDBG("RAINBOW BrokeStudioFirmware received message FILE_GET_INFO\n");
			if (message_size == 3) {
				uint8 const path = this->rx_buffer.at(2);
				uint8 const file = this->rx_buffer.at(3);
				if (path < NUM_FILE_PATHS && file < NUM_FILES && this->file_exists[path][file]) {
					// Compute info
					uint32 file_crc32;
					file_crc32 = CalcCRC32(0L, this->files[path][file].data(), this->files[path][file].size());

					uint32 file_size = this->files[path][file].size();

					// Send info
					this->tx_messages.push_back({
						9,
						static_cast<uint8>(e2n_cmds_t::FILE_INFO),

						static_cast<uint8>((file_crc32 >> 24) & 0xff),
						static_cast<uint8>((file_crc32 >> 16) & 0xff),
						static_cast<uint8>((file_crc32 >> 8) & 0xff),
						static_cast<uint8>(file_crc32 & 0xff),

						static_cast<uint8>((file_size >> 24) & 0xff),
						static_cast<uint8>((file_size >> 16) & 0xff),
						static_cast<uint8>((file_size >> 8 ) & 0xff),
						static_cast<uint8>(file_size & 0xff)
					});
				}else {
					// File not found or path/file out of bounds
					this->tx_messages.push_back({
						1,
						static_cast<uint8>(e2n_cmds_t::FILE_INFO)
					});
				}
			}
			break;
		default:
			UDBG("RAINBOW BrokeStudioFirmware received unknown message %02x\n", this->rx_buffer.at(1));
			break;
	};

	// Remove processed message
	this->rx_buffer.clear();
}

void BrokeStudioFirmware::readFile(uint8 path, uint8 file, uint8 n, uint32 offset) {
	assert(path < NUM_FILE_PATHS);
	assert(file < NUM_FILES);

	// Get data range
	std::vector<uint8> const& f = this->files[path][file];
	std::vector<uint8>::const_iterator data_begin;
	std::vector<uint8>::const_iterator data_end;
	if (offset >= f.size()) {
		data_begin = f.end();
		data_end = data_begin;
	}else {
		data_begin = f.begin() + offset;
		data_end = f.begin() + std::min(static_cast<std::vector<uint8>::size_type>(offset) + n, f.size());
	}
	std::vector<uint8>::size_type const data_size = data_end - data_begin;

	// Write response
	std::deque<uint8> message({
		static_cast<uint8>(data_size + 2),
		static_cast<uint8>(e2n_cmds_t::FILE_DATA),
		static_cast<uint8>(data_size)
	});
	message.insert(message.end(), data_begin, data_end);
	this->tx_messages.push_back(message);
}

template<class I>
void BrokeStudioFirmware::writeFile(uint8 path, uint8 file, uint32 offset, I data_begin, I data_end) {
	std::vector<uint8>& f = this->files[path][file];
	auto const data_size = data_end - data_begin;
	uint32 const offset_end = offset + data_size;
	if (offset_end > f.size()) {
		f.resize(offset_end, 0);
	}

	for (std::vector<uint8>::size_type i = offset; i < offset_end; ++i) {
		f[i] = *data_begin;
		++data_begin;
	}
	this->file_exists[path][file] = true;
}

uint8 BrokeStudioFirmware::getFreeFileId(uint8 path) const {
	uint8 const NOT_FOUND = 128;
	if (path >= NUM_FILE_PATHS) {
		return NOT_FOUND;
	}
	std::array<bool, NUM_FILES> const& existing_files = this->file_exists.at(path);
	for (size_t i = 0; i < existing_files.size(); ++i) {
		if (!existing_files[i]) {
			return i;
		}
	}
	return NOT_FOUND;
}

void BrokeStudioFirmware::saveFiles() const {
	char const* filesystem_file_path = ::getenv("RAINBOW_FILESYSTEM_FILE");
	if (filesystem_file_path == NULL) {
		return;
	}

	std::ofstream ofs(filesystem_file_path);
	for(std::array<bool, NUM_FILES> const& path: this->file_exists) {
		for(bool exists: path) {
			ofs << exists << ' ';
		}
		ofs << '\n';
	}
	ofs << '\n';

	for (std::array<std::vector<uint8>, NUM_FILES> const& path: this->files) {
		for (std::vector<uint8> const& file: path) {
			ofs << file.size() << '\n';
			for (uint8 byte: file) {
				ofs << (uint16_t)byte << ' ';
			}
			ofs << '\n';
		}
	}
}

void BrokeStudioFirmware::loadFiles() {
	char const* filesystem_file_path = ::getenv("RAINBOW_FILESYSTEM_FILE");
	if (filesystem_file_path == NULL) {
		return;
	}

	std::ifstream ifs(filesystem_file_path);
	if (ifs.fail()) {
		return;
	}

	for(std::array<bool, NUM_FILES>& path: this->file_exists) {
		for(bool& exists: path) {
			ifs >> exists;
		}
	}

	for (std::array<std::vector<uint8>, NUM_FILES>& path: this->files) {
		for (std::vector<uint8>& file: path) {
			size_t file_size;
			ifs >> file_size;

			file.clear();
			file.reserve(file_size);
			for (size_t i = 0; i < file_size; ++i) {
				uint16_t byte;
				ifs >> byte;
				if (byte > 255) throw std::runtime_error("invalid filesystem file");
				file.push_back(byte);
			}
		}
	}
}

template<class I>
void BrokeStudioFirmware::sendMessageToServer(I begin, I end) {
#if RAINBOW_DEBUG >= 1
	FCEU_printf("RAINBOW message to send: ");
	for (I cur = begin; cur < end; ++cur) {
		FCEU_printf("%02x ", *cur);
	}
	FCEU_printf("\n");
#endif

	if (this->socket != nullptr) {
		size_t message_size = end - begin;
		std::vector<uint8> aggregated;
		aggregated.reserve(message_size);
		aggregated.insert(aggregated.end(), begin, end);
		this->socket->sendBinary(aggregated);
		this->socket->poll();
	}
}

template<class I>
void BrokeStudioFirmware::sendUdpDatagramToServer(I begin, I end) {
#if RAINBOW_DEBUG >= 1
	FCEU_printf("RAINBOW udp datagram to send: ");
	for (I cur = begin; cur < end; ++cur) {
		FCEU_printf("%02x ", *cur);
	}
	FCEU_printf("\n");
#endif

	if (this->udp_socket != -1) {
		size_t message_size = end - begin;
		std::vector<uint8> aggregated;
		aggregated.reserve(message_size);
		aggregated.insert(aggregated.end(), begin, end);

		ssize_t n = sendto(
			this->udp_socket, cast_network_payload(aggregated.data()), aggregated.size(), 0,
			reinterpret_cast<sockaddr*>(&this->server_addr), sizeof(sockaddr)
		);
		if (n == -1) {
			UDBG("RAINBOW UDP send failed: %s\n", strerror(errno));
		}else if (n != message_size) {
			UDBG("RAINBOW UDP sent partial message\n");
		}
	}
}

void BrokeStudioFirmware::receiveDataFromServer() {
	// Websocket
	if (this->socket != nullptr) {
		this->socket->poll();
		this->socket->dispatchBinary([this] (std::vector<uint8_t> const& data) {
			size_t const msg_len = data.end() - data.begin();
			if (msg_len <= 0xff) {
				UDBG("RAINBOW WebSocket data received... size %02x, msg: ", msg_len);
				for (uint8_t const c: data) {
					UDBG("%02x", c);
				}
				UDBG("\n");
				// add last received byte first to match hardware behavior
				// it's more of a mapper thing though ...
				// needs a dummy $5000 read when reading data from buffer
				std::deque<uint8> message({
					static_cast<uint8>(msg_len+1),
					static_cast<uint8>(e2n_cmds_t::MESSAGE_FROM_SERVER)
				});
				message.insert(message.end(), data.begin(), data.end());
				this->tx_messages.push_back(message);
			}
		});
	}

	// UDP
	if (this->udp_socket != -1) {
		fd_set rfds;
		FD_ZERO(&rfds);
		FD_SET(this->udp_socket, &rfds);

		timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		int n_readable = select(this->udp_socket+1, &rfds, NULL, NULL, &tv);
		if (n_readable == -1) {
			UDBG("RAINBOW failed to check sockets for data: %s\n", strerror(errno));
		}else if (n_readable > 0) {
			if (FD_ISSET(this->udp_socket, &rfds)) {
				size_t const MAX_DGRAM_SIZE = 256;
				std::vector<uint8> data;
				data.resize(MAX_DGRAM_SIZE);
				sockaddr_in addr_from;
				socklen_t addr_from_len = sizeof(addr_from);
				ssize_t msg_len = recvfrom(
					this->udp_socket, cast_network_payload(data.data()), MAX_DGRAM_SIZE, 0,
					reinterpret_cast<sockaddr*>(&addr_from), &addr_from_len
				);
				if (msg_len == -1) {
					UDBG("RAINBOW failed to read UDP socket: %s\n", strerror(errno));
				}else if (msg_len < 256) {
					UDBG("RAINBOW received UDP datagram of size %zd: ", msg_len);
					for (auto it = data.begin(); it != data.begin() + msg_len; ++it) {
						UDBG("%02x", *it);
					}
					UDBG("\n");
					std::deque<uint8> message({
						static_cast<uint8>(msg_len+1),
						static_cast<uint8>(e2n_cmds_t::MESSAGE_FROM_SERVER)
					});
					message.insert(message.end(), data.begin(), data.begin() + msg_len);
					this->tx_messages.push_back(message);
				}else {
					UDBG("RAINBOW received a bigger than expected UDP datagram\n");
					//TODO handle it like Rainbow's ESP handle it
				}
			}
		}
	}
}

void BrokeStudioFirmware::closeConnection() {
	//TODO close UDP socket

	// Do nothing if connection is already closed
	if (this->socket == nullptr) {
		return;
	}

	// Gently ask for connection closing
	if (this->socket->getReadyState() == WebSocket::OPEN) {
		this->socket->close();
	}

	// Start a thread that waits for the connection to be closed, before deleting the socket
	if (this->socket_close_thread.joinable()) {
		this->socket_close_thread.join();
	}
	WebSocket::pointer ws = this->socket;
	this->socket_close_thread = std::thread([ws] {
		while (ws->getReadyState() != WebSocket::CLOSED) {
			ws->poll(5);
		}
		delete ws;
	});

	// Forget about this connection
	this->socket = nullptr;
}

void BrokeStudioFirmware::openConnection() {
	this->closeConnection();

	if (this->active_protocol == server_protocol_t::WEBSOCKET) {
		// Create websocket
		std::ostringstream ws_url;
		ws_url << "ws://" << this->server_settings_address << ':' << this->server_settings_port;
		WebSocket::pointer ws = WebSocket::from_url(ws_url.str());
		if (!ws) {
			UDBG("RAINBOW unable to connect to WebSocket server\n");
		}else {
			this->socket = ws;
		}
	}else {
		// Init UDP socket and store parsed address
		hostent *he = gethostbyname(this->server_settings_address.c_str());
		if (he == NULL) {
			UDBG("RAINBOW unable to resolve UDP server's hostname\n");
			return;
		}
		bzero(reinterpret_cast<void*>(&this->server_addr), sizeof(this->server_addr));
		this->server_addr.sin_family = AF_INET;
		this->server_addr.sin_port = htons(this->server_settings_port);
		this->server_addr.sin_addr = *((in_addr*)he->h_addr);

		this->udp_socket = ::socket(AF_INET, SOCK_DGRAM, 0);
		if (this->udp_socket == -1) {
			UDBG("RAINBOW unable to connect to UDP server: %s\n", strerror(errno));
		}

		sockaddr_in bind_addr;
		bzero(reinterpret_cast<void*>(&bind_addr), sizeof(bind_addr));
		bind_addr.sin_family = AF_INET;
		bind_addr.sin_port = htons(0);
		bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		bind(this->udp_socket, reinterpret_cast<sockaddr*>(&bind_addr), sizeof(sockaddr));
	}
}

void BrokeStudioFirmware::pingRequest(uint8 n) {
	using std::chrono::time_point;
	using std::chrono::steady_clock;
	using std::chrono::duration_cast;
	using std::chrono::milliseconds;

	uint8 min = 255;
	uint8 max = 0;
	uint32 total_ms = 0;
	uint8 lost = 0;

	pping_s* pping = pping_init(this->server_settings_address.c_str());
	if (pping == NULL) {
		lost = n;
	}else {
		for (uint8 i = 0; i < n; ++i) {
			time_point<steady_clock> begin = steady_clock::now();
			int r = pping_ping(pping);
			time_point<steady_clock> end = steady_clock::now();

			if (r != 0) {
				UDBG("RAINBOW BrokeStudioFirmware ping lost packet\n");
				++lost;
			}else {
				uint32 const round_trip_time_ms = duration_cast<milliseconds>(end - begin).count();
				uint8 const rtt = (round_trip_time_ms + 2) / 4;
				UDBG("RAINBOW BrokeStudioFirmware ping %d ms\n", round_trip_time_ms);
				min = std::min(min, rtt);
				max = std::max(max, rtt);
				total_ms += round_trip_time_ms;
			}

			if (i < n - 1) {
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		}
		pping_free(pping);
	}

	this->ping_min = min;
	if (lost < n) {
		this->ping_avg = ((total_ms / (n - lost)) + 2) / 4;
	}
	this->ping_max = max;
	this->ping_lost = lost;
	this->ping_ready = true;
	UDBG("RAINBOW BrokeStudioFirmware ping stored: %d/%d/%d/%d (min/max/avg/lost)\n", this->ping_min, this ->ping_max, this->ping_avg, this->ping_lost);
}

void BrokeStudioFirmware::receivePingResult() {
	if (!this->ping_ready) {
		return;
	}
	assert(this->ping_thread.joinable());

	this->ping_thread.join();
	this->ping_ready = false;

	this->tx_messages.push_back({
		5,
		static_cast<uint8>(e2n_cmds_t::SERVER_PING),
		this->ping_min,
		this->ping_max,
		this->ping_avg,
		this->ping_lost
	});
}

void BrokeStudioFirmware::httpdEvent(mg_connection *nc, int ev, void *ev_data) {
	BrokeStudioFirmware* self = reinterpret_cast<BrokeStudioFirmware*>(nc->mgr->user_data);
	auto send_message = [&] (int status_code, char const * body, char const * mime) {
		std::string header = std::string("Content-Type: ") + mime + "\r\nConnection: close\r\n";
		mg_send_response_line(nc, status_code, header.c_str());
		mg_printf(nc, body);
		nc->flags |= MG_F_SEND_AND_CLOSE;
	};
	auto send_generic_error = [&] {
		send_message(200, "{\"success\":\"false\"}\n", "application/json");
	};
	if (ev == MG_EV_HTTP_REQUEST) {
		UDBG("http request event \n");
		struct http_message *hm = (struct http_message *) ev_data;
		UDBG("  uri: %.*s\n", hm->uri.len, hm->uri.p);
		if (std::string("/api/file/list") == std::string(hm->uri.p, hm->uri.len)) {
			char path[256];
			int const path_len = mg_get_http_var(&hm->query_string, "path", path, 256);
			if (path_len < 0) {
				send_generic_error();
				return;
			}
			mg_send_response_line(nc, 200, "Content-Type: application/json\r\nConnection: close\r\n");
			mg_printf(nc, "[");
			if (path_len == 0) {
				// Send three paths
				for (uint8_t path_index = 0; path_index < NUM_FILE_PATHS; ++path_index) {
					if (path_index != 0)
					{
						mg_printf(nc, ",");
					}
					uint32_t path_size = 0L;
					for (uint8_t file_index = 0; file_index < NUM_FILES; ++file_index) {
						if (self->file_exists[path_index][file_index])
							path_size += self->files[path_index][file_index].size();
					}
					mg_printf(nc, "{\"id\":\"%d\",\"type\":\"dir\",\"name\":\"%s\",\"size\":\"%d\"}", path_index, dir_names[path_index].c_str(), path_size);
				}
			}
			else {
				// Send path content
				int path_index = path[0] - '0';
				mg_printf(nc, "{\"id\":\"\",\"type\":\"dir\",\"name\":\"..\",\"size\":\"-1\"}");
					for (uint8_t file_index = 0; file_index < self->file_exists[path_index].size(); ++file_index) {
						if (self->file_exists[path_index][file_index]) {
							mg_printf(nc, ",");
							mg_printf(nc, "{\"id\":\"%d\",\"type\":\"file\",\"name\":\"file%d.bin\",\"size\":\"%d\"}", file_index, file_index, static_cast<int>(self->files[path_index][file_index].size()));
						}
					}
			}
			mg_printf(nc, "]");
			nc->flags |= MG_F_SEND_AND_CLOSE;
		}
		else if (std::string("/api/file/free") == std::string(hm->uri.p, hm->uri.len)) {
			char path[256];
			int const path_len = mg_get_http_var(&hm->query_string, "path", path, 256);
			if (path_len <= 0) {
				send_generic_error();
				return;
			}
			mg_send_response_line(nc, 200, "Content-Type: application/json\r\nConnection: close\r\n");
			mg_printf(nc, "[");
			int path_index = std::atoi(path);
			bool found = false;
			for (uint8_t file_index = 0; file_index < NUM_FILES; ++file_index) {
				if (!self->file_exists[path_index][file_index]) {
					if (found) {
						mg_printf(nc, ",");
					}
					mg_printf(nc, "{\"id\":\"%d\",\"name\":\"%d\"}", file_index, file_index);
					found = true;
				}
			}
			mg_printf(nc, "]");
			nc->flags |= MG_F_SEND_AND_CLOSE;
		}else if (std::string("/api/file/delete") == std::string(hm->uri.p, hm->uri.len)) {
			char path[256];
			char file[256];
			int const path_len = mg_get_http_var(&hm->query_string, "path", path, 256);
			int const file_len = mg_get_http_var(&hm->query_string, "file", file, 256);

			if (path_len <= 0 || file_len <= 0) {
				send_generic_error();
				return;
			}

			uint8 path_index = std::atoi(path);
			uint8 file_index = std::atoi(file);

			if (path_index >= NUM_FILE_PATHS || file_index >= NUM_FILES || !self->file_exists[path_index][file_index]) {
				send_generic_error();
				return;
			}

			UDBG("RAINBOW Web(self=%p) deleting file %d/%d\n", self, path_index, file_index);
			self->file_exists[path_index][file_index] = false;
			self->files[path_index][file_index].clear();
			self->saveFiles();
			send_message(200, "{\"success\":\"true\"}\n", "application/json");

		}else if (std::string("/api/file/rename") == std::string(hm->uri.p, hm->uri.len)) {
			char path[256];
			char file[256];
			char new_path[256];
			char new_file[256];
			int const path_len = mg_get_http_var(&hm->query_string, "path", path, 256);
			int const file_len = mg_get_http_var(&hm->query_string, "file", file, 256);
			int const new_path_len = mg_get_http_var(&hm->query_string, "newPath", new_path, 256);
			int const new_file_len = mg_get_http_var(&hm->query_string, "newFile", new_file, 256);

			if (path_len <= 0 || file_len <= 0 || new_path_len <= 0 || new_file_len <= 0) {
				send_generic_error();
				return;
			}

			uint8 path_index = std::atoi(path);
			uint8 file_index = std::atoi(file);
			uint8 new_path_index = std::atoi(new_path);
			uint8 new_file_index = std::atoi(new_file);
			UDBG("%d %d %d %d\n", path_index, file_index, new_path_index, new_file_index);

			if (path_index >= NUM_FILE_PATHS || file_index >= NUM_FILES || new_path_index >= NUM_FILE_PATHS || new_file_index >= NUM_FILES|| !self->file_exists[path_index][file_index]) {
				send_generic_error();
				return;
			}

			self->files[new_path_index][new_file_index] = self->files[path_index][file_index];
			self->file_exists[new_path_index][new_file_index] = self->file_exists[path_index][file_index];
			self->file_exists[path_index][file_index] = false;
			self->files[path_index][file_index].clear();
			self->saveFiles();

			send_message(200, "{\"success\":\"true\"}\n", "application/json");
		}else if (std::string("/api/file/download") == std::string(hm->uri.p, hm->uri.len)) {
			char path[256];
			char file[256];
			int const path_len = mg_get_http_var(&hm->query_string, "path", path, 256);
			int const file_len = mg_get_http_var(&hm->query_string, "file", file, 256);

			if (path_len <= 0 ||file_len <= 0) {
				send_generic_error();
				return;
			}

			uint8 path_index = std::atoi(path);
			uint8 file_index = std::atoi(file);

			if (path_index >= NUM_FILE_PATHS || file_index >= NUM_FILES || !self->file_exists[path_index][file_index]) {
				send_generic_error();
				return;
			}
			mg_send_response_line(
				nc, 200,
				"Content-Type: application/octet-stream\r\n"
				"Connection: close\r\n"
			);
			mg_send(nc, self->files[path_index][file_index].data(), self->files[path_index][file_index].size());
			nc->flags |= MG_F_SEND_AND_CLOSE;
		}else if (std::string("/api/file/upload") == std::string(hm->uri.p, hm->uri.len)) {
			// Get boundary for multipart form in HTTP headers
			std::string multipart_boundary;
			{
				mg_str const * content_type = mg_get_http_header(hm, "Content-Type");
				if (content_type == NULL) {
					send_generic_error();
					return;
				}
				static std::regex const content_type_regex("multipart/form-data; boundary=(.*)");
				std::smatch match;
				std::string content_type_str(content_type->p, content_type->len);
				if (!std::regex_match(content_type_str, match, content_type_regex)) {
					send_generic_error();
					return;
				}
				assert(match.size() == 2);
				multipart_boundary = match[1];
			}

			// Parse form parts
			std::map<std::string, std::string> params;
			{
				std::string body_str(hm->body.p, hm->body.len);
				std::string::size_type pos = 0;
				while (pos != std::string::npos) {
					// Find the parameter name
					std::string::size_type found_pos = body_str.find("form-data; name=\"", pos);
					if (found_pos == std::string::npos) {
						break;
					}
					pos = found_pos + 17;
					found_pos = body_str.find('"', pos);
					if (found_pos == std::string::npos) {
						break;
					}
					std::string const param_name = body_str.substr(pos, found_pos - pos);
					pos = found_pos;

					// Find the begining of the body
					found_pos = body_str.find("\r\n\r\n", pos);
					if (found_pos == std::string::npos) {
						break;
					}
					pos = found_pos + 4;

					// Find the begining of the next delimiter
					found_pos = body_str.find("\r\n--" + multipart_boundary, pos);
					if (found_pos == std::string::npos) {
						break;
					}
					std::string const param_value = body_str.substr(pos, found_pos - pos);
					pos = found_pos;

					// Store parsed parameter
					params[param_name] = param_value;
				}
			}

			// Process request
			std::map<std::string, std::string>::const_iterator file_data = params.find("file_data");
			std::map<std::string, std::string>::const_iterator path = params.find("path");
			std::map<std::string, std::string>::const_iterator file = params.find("file");

			if (file_data == params.end() || path == params.end() || file == params.end()) {
				send_generic_error();
				return;
			}

			uint8 path_index = std::atoi(path->second.c_str());
			uint8 file_index = std::atoi(file->second.c_str());

			self->files[path_index][file_index] = std::vector<uint8>(file_data->second.begin(), file_data->second.end());
			self->file_exists[path_index][file_index] = true;
			self->saveFiles();

			UDBG("RAINBOW Web(self=%p) sucessfuly uploaded file %d/%d\n", self, path_index, file_index);

			// Return something webbrowser friendly
			send_message(200, "{\"success\":\"true\"}\n", "application/json");
		}else {
			char const* www_root = ::getenv("RAINBOW_WWW_ROOT");
			if (www_root == NULL) {
				std::string upload_form = R"-(<!doctype html><html><head><style>*{margin:2px}body{font-family:Arial}</style></style>)-";
				upload_form += R"-(<body><h1>Upload file to Rainbow:</h1>)-";
				upload_form += R"-(<input id="file_data" name="file_data" type="file"><br/>)-";
				upload_form += R"-(Path: <select id="path" name="path"><option value="0">SAVE</option><option value="1">ROMS</option><option value="2">USER</option></select><br />)-";
				upload_form += R"-(File: <select id="file" name="file">)-";
				for (uint8 i = 0; i < NUM_FILES; i++)
				{
					upload_form += "<option value=\"" + std::to_string(i) + "\">" + std::to_string(i) + "</option>";
				}
				upload_form += "</select><br/>";
				upload_form += R"-(<button id="btnSubmit" type="submit" onclick="handleSubmit()">Upload</button>)-";
				upload_form += "<script>function handleSubmit(){";
				upload_form += R"-(btnSubmit.disabled=true;)-";
				upload_form += R"-(btnSubmit.innerHTML = "Uploading...";)-";
				upload_form += R"-(let formData = new FormData();)-";
				upload_form += R"-(formData.append("file_data", file_data.files[0]);)-";
				upload_form += R"-(let path_index = path.options[path.selectedIndex].value;)-";
				upload_form += R"-(formData.append("path", path.value);)-";
				upload_form += R"-(formData.append("file", file.value);)-";
				upload_form += R"-(let xhr = new XMLHttpRequest();)-";
				upload_form += R"-(xhr.open("POST", "/api/file/upload");)-";
				upload_form += R"-(xhr.onreadystatechange = function () {)-";
				upload_form += R"-(if (this.readyState === XMLHttpRequest.DONE && this.status === 200) {)-";
				upload_form += R"-(var r = JSON.parse(this.responseText);)-";
				upload_form += R"-(if (r.success) alert("File uploaded successfully.");)-";
				upload_form += R"-(else alert('Error while uploading the file.');)-";
				upload_form += R"-(btnSubmit.disabled=false;)-";
				upload_form += R"-(btnSubmit.innerHTML = "Upload";)-";
				upload_form += R"-(})-";
				upload_form += R"-(if (this.readyState === XMLHttpRequest.DONE && this.status === 0) {)-";
				upload_form += R"-(alert('Please check your connection.');)-";
				upload_form += R"-(btnSubmit.disabled=false;)-";
				upload_form += R"-(btnSubmit.innerHTML = "Upload";)-";
				upload_form += R"-(})-";
				upload_form += R"-(};)-";
				upload_form += R"-(xhr.send(formData);)-";
				upload_form += "}</script>";
				upload_form += "</body></html>";
				send_message(200, upload_form.c_str(), "text/html");
			}else {
				// Translate url path to a file path on disk
				assert(hm->uri.len > 0); // Impossible as HTTP header are constructed in such a way that there is always at least one char in path (I think)
				std::string uri(hm->uri.p+1, hm->uri.len-1); // remove leading "/"
				if (uri.empty()) {
					uri = "index.html";
				}

				// Try to serve requested file, if not found, then try to serve index.html
				std::string file_path = "";
				try {
					file_path = std::string(www_root) + uri;
					std::vector<uint8> contents = readHostFile(file_path);
				}catch (std::runtime_error const& e) {
					try {
						file_path = std::string(www_root) + "index.html";
						std::vector<uint8> contents = readHostFile(file_path);
					}catch (std::runtime_error const& e) {
						std::string message(
							"<html><body><h1>404</h1><p>" +
							file_path +
							"</p><p>" +
							e.what() +
							"</p></body></html>"
						);
						send_message(
							404,
							message.c_str(),
							"text/html"
						);
					}
				}

				// Serve file
				std::vector<uint8> contents = readHostFile(file_path);
				// Basic / naive mime type handler
				std::string ext = file_path.substr(file_path.find_last_of(".") + 1);
				std::string mime_type = "";
				if (ext == "html") mime_type = "text/html";
				else if (ext == "css") mime_type = "text/css";
				else if (ext == "js") mime_type = "application/javascript";
				else mime_type = "application/octet-stream";
				std::string content_type(
					"Content-Type: " + mime_type + "\r\n" +
					"Connection: close\r\n"
				);
				mg_send_response_line(
					nc, 200, content_type.c_str()
				);
				mg_send(nc, contents.data(), contents.size());
				nc->flags |= MG_F_SEND_AND_CLOSE;
			}
		}
	}
}
