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
	virtual void setGpio15(bool v) = 0;
	virtual bool getGpio15() = 0;
};

//////////////////////////////////////
// Glutock's ESP firmware implementation

class GlutockFirmware: public EspFirmware {
public:
	GlutockFirmware();
	~GlutockFirmware();

	void rx(uint8 v) override;
	uint8 tx() override;

	virtual void setGpio15(bool v) override;
	virtual bool getGpio15() override;

private:
	// Defined message types from CPU to ESP
	enum class n2e_cmds_t : uint8 {
		UNUSED_0,
		DEBUG_LOG,
		CLEAR_BUFFERS,
		GET_WIFI_STATUS,
		GET_SERVER_STATUS,
		CONNECT_TO_SERVER,
		DISCONNECT_FROM_SERVER,
		SEND_MESSAGE_TO_SERVER,
	};

	// Defined message types from ESP to CPU
	enum class e2n_cmds_t : uint8 {
		UNUSED_0,
		UNUSED_1,
		UNUSED_2,
		WIFI_STATUS,
		SERVER_STATUS,
		UNUSED_3,
		UNUSED_4,
		MESSAGE_FROM_SERVER,
	};

	void processBufferedMessage();

	template<class I>
	void sendMessageToServer(I begin, I end);
	void receiveDataFromServer();

private:
	std::deque<uint8> rx_buffer;
	std::deque<uint8> tx_buffer;

	WebSocket::pointer socket = nullptr;

	bool msg_first_byte = true;
	uint8 msg_length = 0;
	uint8 last_byte_read = 0;
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
	UDBG("UNICORN GlutockFirmware rx %02x\n", v);
	if (this->msg_first_byte) {
		this->msg_first_byte = false;
		this->msg_length = v + 1;
	}
	this->rx_buffer.push_back(v);

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
	if (!this->tx_buffer.empty()) {
		last_byte_read = this->tx_buffer.front();
		this->tx_buffer.pop_front();
	}

	UDBG("UNICORN GlutockFirmware tx <= %02x\n", last_byte_read);
	return last_byte_read;
}

void GlutockFirmware::setGpio15(bool /*v*/) {
}

bool GlutockFirmware::getGpio15() {
	this->receiveDataFromServer();
	return !this->tx_buffer.empty();
}

void GlutockFirmware::processBufferedMessage() {
	if (this->rx_buffer.size() >= 2) {
		// Process the message in RX buffer
		switch (static_cast<n2e_cmds_t>(this->rx_buffer.at(1))) {
			case n2e_cmds_t::UNUSED_0:
				UDBG("UNICORN GlutockFirmware received message NULL\n");
				break;
			case n2e_cmds_t::DEBUG_LOG:
				#ifdef UNICORN_DBG
					FCEU_printf("UNICORN DEBUG/LOG: ");
					for (std::deque<uint8>::const_iterator cur = this->rx_buffer.begin() + 2; cur < this->rx_buffer.end(); ++cur) {
						FCEU_printf("%02x ", *cur);
					}
					FCEU_printf("\n");
				#endif
				break;
			case n2e_cmds_t::CLEAR_BUFFERS:
				// TODO : clean tx / rx buffers
				break;
			case n2e_cmds_t::GET_WIFI_STATUS:
				UDBG("UNICORN GlutockFirmware received message GET_WIFI_STATUS\n");
				this->tx_buffer.push_back(last_byte_read);
				this->tx_buffer.push_back(2);
				this->tx_buffer.push_back(static_cast<uint8>(e2n_cmds_t::WIFI_STATUS));
				this->tx_buffer.push_back(3); // Simple answer, wifi is ok
				break;
			case n2e_cmds_t::GET_SERVER_STATUS:
				UDBG("UNICORN GlutockFirmware received message GET_SERVER_STATUS\n");
				this->tx_buffer.push_back(last_byte_read);
				this->tx_buffer.push_back(2);
				this->tx_buffer.push_back(static_cast<uint8>(e2n_cmds_t::SERVER_STATUS));
				this->tx_buffer.push_back(this->socket != nullptr); // Server connection is ok if we succeed to open it
				break;
			case n2e_cmds_t::SEND_MESSAGE_TO_SERVER: {
				UDBG("UNICORN GlutockFirmware received message SEND_MESSAGE\n");
				uint8 const payload_size = this->rx_buffer.size() - 2;
				std::deque<uint8>::const_iterator payload_begin = this->rx_buffer.begin() + 2;
				std::deque<uint8>::const_iterator payload_end = payload_begin + payload_size;
				this->sendMessageToServer(payload_begin, payload_end);
				break;
			}
			default:
				UDBG("UNICORN GlutockFirmware received unknown message %02x\n", this->rx_buffer.at(1));
				break;
		};

		// Remove processed message
		std::deque<uint8>::size_type message_size = this->rx_buffer.front() + 1;
		if (message_size > this->rx_buffer.size()) {
			message_size = this->rx_buffer.size();
		}
		this->rx_buffer.erase(this->rx_buffer.begin(), this->rx_buffer.begin() + message_size);

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
			UDBG("UNICORN WebSocket data received...\n");
			// add last received byte first to match hardware behavior
			// it's more of a mapper thing though ...
			// needs a dummy $5000 read when reading data from buffer
			this->tx_buffer.push_back(last_byte_read);
			this->tx_buffer.push_back(static_cast<uint8>(msg_len+1));
			this->tx_buffer.push_back(static_cast<uint8>(e2n_cmds_t::MESSAGE_FROM_SERVER));
			this->tx_buffer.insert(this->tx_buffer.end(), data.begin(), data.end());
		}
	});
}

//////////////////////////////////////
// Mapper implementation

static uint8 *WRAM = NULL;
static uint32 WRAMSIZE;
static EspFirmware *esp = NULL;
static bool esp_enable = true;
static bool irq_enable = true;

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

static DECLFW(UNICORNWriteFlags) {
	UDBG("UNICORN write %04x %02x\n", A, V);
	esp_enable = V & 0x01;
	irq_enable = V & 0x40;
}

static DECLFR(UNICORNReadFlags) {
	UDBG("UNICORN read flags %04x\n", A);
	uint8 esp_rts_flag = esp->getGpio15() ? 0x80 : 0x00;
	uint8 esp_enable_flag = esp_enable ? 0x01 : 0x00;
	uint8 irq_enable_flag = irq_enable ? 0x40 : 0x00;
	return esp_rts_flag | esp_enable_flag | irq_enable_flag;
}

static void UNICORNMapIrq(int32) {
	if (irq_enable) {
		if (esp->getGpio15()) {
			X6502_IRQBegin(FCEU_IQEXT);
		} else {
			X6502_IRQEnd(FCEU_IQEXT);
		}
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
	SetWriteHandler(0x5001, 0x5001, UNICORNWriteFlags);
	SetReadHandler(0x5001, 0x5001, UNICORNReadFlags);

	esp = new GlutockFirmware;
	esp_enable = true;
	irq_enable = true;
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
	MapIRQHook = UNICORNMapIrq;
}
