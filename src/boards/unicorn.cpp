#include "mapinc.h"
#include "../ines.h"

#ifdef WIN32
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#endif

#include <deque>
#include <sstream>

#undef UNICORN_DEBUG

#ifdef UNICORN_DEBUG
#define UDBG(...) FCEU_printf(__VA_ARGS__)
#else
#define UDBG(...)
#endif

class EspFirmware {
public:
	virtual ~EspFirmware() = default;

	// Communication pins (dont care about UART details, directly transmit bytes)
	virtual void rx(uint8 v) = 0;
	virtual uint8 tx() = 0;

	// General purpose I/O pins
	virtual void setGpio0(bool v) = 0;
	virtual bool getGpio0() = 0;
	virtual void setGpio2(bool v) = 0;
	virtual bool getGpio2() = 0;
};

class GlutockFirmware: public EspFirmware {
public:
	GlutockFirmware();
	~GlutockFirmware();

	void rx(uint8 v) override;
	uint8 tx() override;

	virtual void setGpio0(bool v) override;
	virtual bool getGpio0() override;
	virtual void setGpio2(bool v) override;
	virtual bool getGpio2() override;

private:
	enum class message_id_t : uint8 {
		MSG_NULL = 0x00,
		MSG_GET_WIFI_STATUS = 0x01,
		MSG_GET_SERVER_STATUS = 0x02,
		MSG_SEND_MESSAGE = 0x03,
	};

	void processBufferedMessages();

	template<class I>
	void sendMessageToServer(I begin, I end);
	void receiveDataFromServer();

private:
	std::deque<uint8> rx_buffer;
	std::deque<uint8> tx_buffer;

	int socket = -1;
};

GlutockFirmware::GlutockFirmware() {
	UDBG("UNICORN GlutockFirmware ctor\n");

	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int sfd, s;

	char* host = "localhost";
	int port = 1234;

	// Resolve address
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;
	std::ostringstream port_str;
	port_str << port;
	s = getaddrinfo(host, port_str.str().c_str(), &hints, &result);
	if (s != 0) {
		UDBG("UNICORN unable to resolve address\n");
		return;
	}

	// Connect to server
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = ::socket(rp->ai_family, rp->ai_socktype,
				rp->ai_protocol);
		if (sfd == -1) {
			continue;
		}

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}

		close(sfd);
	}
	freeaddrinfo(result);
	if (rp == NULL) {
		UDBG("UNICORN failed to connect to server\n");
		return;
	}

	// Set socket in non-blocking mode
	bool success = false;
#ifdef _WIN32
	unsigned long mode = 0;
	if (ioctlsocket(sfd, FIONBIO, &mode) == 0) {
		success = true;
	}
#else
	int flags = fcntl(sfd, F_GETFL, 0);
	if (flags != -1) {
		flags |= O_NONBLOCK;
		if (fcntl(sfd, F_SETFL, flags) == 0) {
			success = true;
		}
	}
#endif
	if (!success) {
		UDBG("UNICORN failed to set socket in non-blocking mode");
		close(sfd);
		return;
	}

	// Store socket for later use
	this->socket = sfd;
}

GlutockFirmware::~GlutockFirmware() {
	UDBG("UNICORN GlutockFirmware dtor\n");
	if (this->socket != -1) {
		::close(this->socket);
		this->socket = -1;
	}
}

void GlutockFirmware::rx(uint8 v) {
	UDBG("UNICORN GlutockFirmware rx %02x\n", v);

	this->rx_buffer.push_back(v);
	this->processBufferedMessages();
}

uint8 GlutockFirmware::tx() {
	UDBG("UNICORN GlutockFirmware tx\n");

	// Refresh buffer from network
	this->receiveDataFromServer();

	// Get byte from buffer
	uint8 result;
	if (this->tx_buffer.empty()) {
		result = 0;
	}else {
		result = this->tx_buffer.front();
		this->tx_buffer.pop_front();
	}

	UDBG("UNICORN GlutockFirmware tx <= %02x\n", result);
	return result;
}

void GlutockFirmware::setGpio0(bool /*v*/) {
}

bool GlutockFirmware::getGpio0() {
	this->receiveDataFromServer();
	return !this->tx_buffer.empty();
}

void GlutockFirmware::setGpio2(bool /*v*/) {
}

bool GlutockFirmware::getGpio2() {
	return 0;
}

void GlutockFirmware::processBufferedMessages() {
	/* This function process all messages from the RX buffer until
	 * the buffer is empty or one message is icomplete.
	 */

	bool stop = false; // The processing of a message can set this flag to indicate that the message is incomplete, so processing has to stop
	while (!this->rx_buffer.empty() && !stop) {
		std::deque<uint8>::size_type message_size = 0; // The processing of a message must set this value to the entire message size, to be able to remove it from buffer
		switch (static_cast<message_id_t>(this->rx_buffer.front())) {
			case message_id_t::MSG_NULL:
				UDBG("UNICORN GlutockFirmware received message NULL\n");
				message_size = 1;
				break;
			case message_id_t::MSG_GET_WIFI_STATUS:
				UDBG("UNICORN GlutockFirmware received message GET_WIFI_STATUS\n");
				this->tx_buffer.push_back(1); // Simple answer, wifi is ok
				message_size = 1;
				break;
			case message_id_t::MSG_GET_SERVER_STATUS:
				UDBG("UNICORN GlutockFirmware received message GET_SERVER_STATUS\n");
				this->tx_buffer.push_back(this->socket != -1); // Server connection is ok if we succeed to open it
				message_size = 1;
				break;
			case message_id_t::MSG_SEND_MESSAGE: {
				if (this->rx_buffer.size() < 2) {
					stop = true;
					return;
				}
				uint8 const payload_size = this->rx_buffer[1];
				if (this->rx_buffer.size() < payload_size + 2) {
					stop = true;
					return;
				}
				UDBG("UNICORN GlutockFirmware received message SEND_MESSAGE\n");
				std::deque<uint8>::const_iterator payload_begin = this->rx_buffer.begin() + 2;
				std::deque<uint8>::const_iterator payload_end = payload_begin + payload_size;
				this->sendMessageToServer(payload_begin, payload_end);
				message_size = payload_size + 2;
				break;
			}
			default:
				UDBG("UNICORN GlutockFirmware received unknown message %02x\n", this->rx_buffer.front());
				message_size = 1;
				break;
		};

		// Remove processed message
		assert(stop || message_size != 0); // message size of zero removes no bytes from buffer, if stop is not set it will trigger an infinite loop
		if (message_size != 0) {
			assert(message_size <= this->rx_buffer.size()); // cannot remove more bytes than what is in the buffer
			this->rx_buffer.erase(this->rx_buffer.begin(), this->rx_buffer.begin() + message_size);
		}
	}
}

template<class I>
void GlutockFirmware::sendMessageToServer(I begin, I end) {
#ifdef UNICORN_DBG
	FCEU_printf("UNICORN message to send: ");
	for (I cur = begin; cur < end; ++cur) {
		FCEU_printf("%02x ", *cur);
	}
	FCEU_printf("\n");
#endif

	if (this->socket != -1) {
		size_t message_size = end - begin;
		uint8 aggregated[message_size];
		std::copy(begin, end, aggregated);
		write(this->socket, aggregated, message_size);
	}
}

void GlutockFirmware::receiveDataFromServer() {
	if (this->socket < 0) {
		return;
	}

	uint8 network_data[256];
	ssize_t const read_result = read(this->socket, network_data, 256);
	if (read_result < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		UDBG("UNICORN failed to read from network\n");
		::close(this->socket);
		this->socket = -1;
	}
	if (read_result > 0) {
		UDBG("UNICORN got %d bytes message from server\n", read_result);
		this->tx_buffer.insert(this->tx_buffer.end(), network_data, network_data + read_result);
	}
}

static uint8 *WRAM = NULL;
static uint32 WRAMSIZE;
static EspFirmware *esp = NULL;

static void LatchClose(void) {
	UDBG("UNICORN latch close\n");
	if (WRAM)
		FCEU_gfree(WRAM);
	WRAM = NULL;

	delete esp;
}

static DECLFW(UNICORNWrite) {
	UDBG("UNICORN write %04x %02x\n", A, V);
	esp->rx(V);
}

static DECLFR(UNICORNRead) {
	UDBG("UNICORN read %04x\n", A);
	return esp->tx();
}

static DECLFR(UNICORNReadFlags) {
	UDBG("UNICORN read flags %04x\n", A);
	return esp->getGpio0() ? 0x80 : 0x00;
}

static void UNICORNPower(void) {
	UDBG("UNICORN power\n");
	setprg8r(0x10, 0x6000, 0);	// Famili BASIC (v3.0) need it (uses only 4KB), FP-BASIC uses 8KB
	setprg16(0x8000, ~1);
	setprg16(0xC000, ~0);
	setchr8(0);

	SetReadHandler(0x6000, 0x7FFF, CartBR);
	SetWriteHandler(0x6000, 0x7FFF, CartBW);
	SetReadHandler(0x8000, 0xFFFF, CartBR);

	FCEU_CheatAddRAM(WRAMSIZE >> 10, 0x6000, WRAM);

	SetWriteHandler(0x5000, 0x5000, UNICORNWrite);
	SetReadHandler(0x5000, 0x5000, UNICORNRead);
	SetReadHandler(0x5001, 0x5001, UNICORNReadFlags);

	esp = new GlutockFirmware;
}

void UNICORN_Init(CartInfo *info) {
	UDBG("UNICORN init\n");
	info->Power = UNICORNPower;
	info->Close = LatchClose;

	WRAMSIZE = 8192;
	WRAM = (uint8*)FCEU_gmalloc(WRAMSIZE);
	SetupCartPRGMapping(0x10, WRAM, WRAMSIZE, 1);
	if (info->battery) {
		info->SaveGame[0] = WRAM;
		info->SaveGameLen[0] = WRAMSIZE;
	}
	AddExState(WRAM, WRAMSIZE, 0, "WRAM");
}
