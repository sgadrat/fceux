#include "mapinc.h"
#include "../ines.h"

#include "easywsclient.hpp"
//#include "easywsclient.cpp"

#include <deque>
#include <sstream>

using easywsclient::WebSocket;

#undef UNICORN_DEBUG

#ifdef UNICORN_DEBUG
#define UDBG(...) FCEU_printf(__VA_ARGS__)
#else
#define UDBG(...)
#endif

//////////////////////////////////////
// Abstract ESP firmware interface

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

//////////////////////////////////////
// Glutock's ESP firmware implementation

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

	WebSocket::pointer socket = nullptr;
};

GlutockFirmware::GlutockFirmware() {
	UDBG("UNICORN GlutockFirmware ctor\n");

	WebSocket::pointer ws = WebSocket::from_url("ws://localhost:8126/foo");
	if (!ws) {
		UDBG("UNICORN unable to connect to server");
		return;
	}
	this->socket = ws;
}

GlutockFirmware::~GlutockFirmware() {
	UDBG("UNICORN GlutockFirmware dtor\n");
	if (this->socket != nullptr) {
		delete this->socket;
		this->socket = nullptr;
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
				this->tx_buffer.push_back(this->socket != nullptr); // Server connection is ok if we succeed to open it
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

	if (this->socket != nullptr) {
		size_t message_size = end - begin;
		std::vector<uint8> aggregated;
		aggregated.reserve(message_size);
		aggregated.insert(aggregated.end(), begin, end);
		this->socket->sendBinary(aggregated);
		this->socket->poll();
	}
}

void GlutockFirmware::receiveDataFromServer() {
	if (this->socket == nullptr) {
		return;
	}

	this->socket->poll();
	this->socket->dispatchBinary([this] (std::vector<uint8_t> const& data) {
		this->tx_buffer.insert(this->tx_buffer.end(), data.begin(), data.end());
	});
}

//////////////////////////////////////
// Mapper implementation

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
