#include "mapinc.h"
#include "../ines.h"

#include <deque>

class EspFirmware {
public:
	virtual ~EspFirmware() = default;

	// Communication pins (dont care about UART details, directly transmit bytes)
	virtual void rx(uint8 v);
	virtual uint8 tx();

	// General purpose I/O pins
	void setGpio0(bool v);
	bool getGpio0();
	void setGpio2(bool v);
	bool getGpio2();

private:
	bool gpio0 = false;
	bool gpio2 = false;
};

void EspFirmware::rx(uint8 v) {
	FCEU_printf("EspFirmware TX %02x\n", v);
}

uint8 EspFirmware::tx() {
	FCEU_printf("EspFirmware RX\n");
	return 0;
}

void EspFirmware::setGpio0(bool v) {
	this->gpio0 = v;
}

bool EspFirmware::getGpio0() {
	return this->gpio0;
}

void EspFirmware::setGpio2(bool v) {
	this->gpio2 = v;
}

bool EspFirmware::getGpio2() {
	return this->gpio2;
}

class GlutockFirmware: public EspFirmware {
public:
	GlutockFirmware();
	~GlutockFirmware();

	void rx(uint8 v) override;
	uint8 tx() override;

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

private:
	std::deque<uint8> rx_buffer;
	std::deque<uint8> tx_buffer;
};

GlutockFirmware::GlutockFirmware() {
	//TODO initialize network stuff
	FCEU_printf("UNICORN GlutockFirmware ctor\n");
}

GlutockFirmware::~GlutockFirmware() {
	//TODO clear network stuff
	FCEU_printf("UNICORN GlutockFirmware dtor\n");
}

void GlutockFirmware::rx(uint8 v) {
	FCEU_printf("UNICORN GlutockFirmware rx %02x\n", v);

	this->rx_buffer.push_back(v);
	this->processBufferedMessages();
	this->setGpio0(!this->tx_buffer.empty());
}

uint8 GlutockFirmware::tx() {
	FCEU_printf("UNICORN GlutockFirmware tx\n");

	//TODO Refresh buffer from network

	// Get byte from buffer
	uint8 result;
	if (this->tx_buffer.empty()) {
		result = 0;
	}else {
		result = this->tx_buffer.front();
		this->tx_buffer.pop_front();
	}

	// Update gpio
	this->setGpio0(!this->tx_buffer.empty());

	FCEU_printf("UNICORN GlutockFirmware tx <= %02x\n", result);
	return result;
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
				FCEU_printf("UNICORN GlutockFirmware received message NULL\n");
				message_size = 1;
				break;
			case message_id_t::MSG_GET_WIFI_STATUS:
				FCEU_printf("UNICORN GlutockFirmware received message GET_WIFI_STATUS\n");
				this->tx_buffer.push_back(1); // Simple answer, wifi is ok
				message_size = 1;
				break;
			case message_id_t::MSG_GET_SERVER_STATUS:
				FCEU_printf("UNICORN GlutockFirmware received message GET_SERVER_STATUS\n");
				this->tx_buffer.push_back(0); // Simple answer, server is not connected
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
				FCEU_printf("UNICORN GlutockFirmware received message SEND_MESSAGE\n");
				std::deque<uint8>::const_iterator payload_begin = this->rx_buffer.begin() + 2;
				std::deque<uint8>::const_iterator payload_end = payload_begin + payload_size;
				this->sendMessageToServer(payload_begin, payload_end);
				message_size = payload_size + 2;
				break;
			}
			default:
				FCEU_printf("UNICORN GlutockFirmware received unknown message %02x\n", this->rx_buffer.front());
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
	//TODO actually send data over the network
	FCEU_printf("message to send: ");
	for (I cur = begin; cur < end; ++cur) {
		FCEU_printf("%02x ", *cur);
	}
	FCEU_printf("\n");
}

static uint8 *WRAM = NULL;
static uint32 WRAMSIZE;
static EspFirmware *esp = NULL;

static void LatchClose(void) {
	FCEU_printf("UNICORN latch close\n");
	if (WRAM)
		FCEU_gfree(WRAM);
	WRAM = NULL;

	delete esp;
}

static DECLFW(UNICORNWrite) {
	FCEU_printf("UNICORN write %04x %02x\n", A, V);
	esp->rx(V);
}

static DECLFR(UNICORNRead) {
	FCEU_printf("UNICORN read %04x\n", A);
	return esp->tx();
}

static DECLFR(UNICORNReadFlags) {
	FCEU_printf("UNICORN read flags %04x\n", A);
	return esp->getGpio0() ? 0x80 : 0x00;
}

static void UNICORNPower(void) {
	FCEU_printf("UNICORN power\n");
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
	FCEU_printf("UNICORN init\n");
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
