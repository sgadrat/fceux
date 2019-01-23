#include "mapinc.h"
#include "../ines.h"

#include "easywsclient.hpp"

#include <deque>
#include <sstream>

using easywsclient::WebSocket;

#undef UNICORN_DEBUG
#define UNICORN_DEBUG

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
		MSG_DEBUG_LOG = 0x01,
		MSG_GET_WIFI_STATUS = 0x02,
		MSG_GET_SERVER_STATUS = 0x03,
		MSG_SEND_MESSAGE = 0x04,
	};

	enum class server_message_id_t : uint8 {
		MSG_NULL = 0x00,
		MSG_DEBUG_LOG = 0x01,
		MSG_GET_WIFI_STATUS = 0x02,
		MSG_GET_SERVER_STATUS = 0x03,
		MSG_GOT_MESSAGE = 0x04,
	};

	void processBufferedMessage();

	template<class I>
	void sendMessageToServer(I begin, I end);
	void receiveDataFromServer();

private:
	std::deque<uint8> rx_buffer;
	std::deque<uint8> tx_buffer;

	WebSocket::pointer socket = nullptr;

	uint8 msg_length = 0;
	bool msg_first_byte = true;
};

GlutockFirmware::GlutockFirmware() {
	UDBG("UNICORN GlutockFirmware ctor\n");
	WebSocket::pointer ws = WebSocket::from_url("ws://localhost:3000");
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
	if (this->msg_first_byte) {
		this->msg_first_byte = false;
		this->msg_length = v;
		UDBG("UNICORN RX first byte: %d\n", v);
	} else {
		this->rx_buffer.push_back(v);
		UDBG("UNICORN RX buffer size: %d\n", this->rx_buffer.size());
	}

	if (this->rx_buffer.size() == msg_length) {
		this->processBufferedMessage();
		this->msg_first_byte = true;
	}
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

void GlutockFirmware::processBufferedMessage() {
	if (!this->rx_buffer.empty()) {
		// Process the message in RX buffer
		switch (static_cast<message_id_t>(this->rx_buffer.front())) {
			case message_id_t::MSG_NULL:
				UDBG("UNICORN GlutockFirmware received message NULL\n");
				break;
			case message_id_t::MSG_DEBUG_LOG:
				UDBG("UNICORN GlutockFirmware received message DEBUG_LOG\n");
				break;
			case message_id_t::MSG_GET_WIFI_STATUS:
				UDBG("UNICORN GlutockFirmware received message GET_WIFI_STATUS\n");
				this->tx_buffer.push_back(2);
				this->tx_buffer.push_back(static_cast<uint8>(server_message_id_t::MSG_GET_WIFI_STATUS);
				this->tx_buffer.push_back(1); // Simple answer, wifi is ok
				break;
			case message_id_t::MSG_GET_SERVER_STATUS:
				UDBG("UNICORN GlutockFirmware received message GET_SERVER_STATUS\n");
				this->tx_buffer.push_back(2);
				this->tx_buffer.push_back(static_cast<uint8>(server_message_id_t::MSG_GET_SERVER_STATUS);
				this->tx_buffer.push_back(this->socket != nullptr); // Server connection is ok if we succeed to open it
				break;
			case message_id_t::MSG_SEND_MESSAGE: {
				uint8 const payload_size = this->rx_buffer.size() - 1;
				UDBG("UNICORN GlutockFirmware received message SEND_MESSAGE\n");
				std::deque<uint8>::const_iterator payload_begin = this->rx_buffer.begin() + 1;
				std::deque<uint8>::const_iterator payload_end = payload_begin + payload_size;
				this->sendMessageToServer(payload_begin, payload_end);
				break;
			}
			default:
				UDBG("UNICORN GlutockFirmware received unknown message %02x\n", this->rx_buffer.front());
				break;
		};

		// Remove processed message
		this->rx_buffer.resize(0);
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
		size_t const msg_len = data.end() - data.begin();
		if (msg_len <= 0xff) {
			this->tx_buffer.push_back(static_cast<uint8>(msg_len));
			this->tx_buffer.push_back(static_cast<uint8>(server_message_id_t::MSG_GOT_MESSAGE);
			this->tx_buffer.insert(this->tx_buffer.end(), data.begin(), data.end());
		}
	});
}

//////////////////////////////////////
// Mapper implementation

static uint8 *WRAM = NULL;
static uint32 WRAMSIZE;
static EspFirmware *esp = NULL;
static last_gpio_state = false;

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
	return last_gpio_state ? 0x80 : 0x00;
}

static void UNICORN_hb(void) {
	bool const current_gpio_state = esp->getGpio0();
	if (current_gpio_state && !last_gpio_state) {
		X6502_IRQBegin(FCEU_IQEXT);
	}
	last_gpio_state = current_gpio_state;
}

static void UNICORN_hb(void) {
	// TODO
	//  Find something to avoid repeteadly interrupting
	//  Add possibility to disable interrupt
	if (esp->getGpio0()) {
		X6502_IRQBegin(FCEU_IQEXT);
	}
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
	last_gpio_state = false;
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

	// Set a hook on hblank to be able periodically check if we have to send an interupt
	GameHBIRQHook = UNICORN_hb;
}
