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
	//TODO store byte in buffer, try to parse message from buffer, process parsed message
	FCEU_printf("UNICORN GlutockFirmware rx %02x\n", v);
}

uint8 GlutockFirmware::tx() {
	//TODO refresh buffer from network and get byte from buffer
	FCEU_printf("UNICORN GlutockFirmware tx\n");
	return 0;
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
