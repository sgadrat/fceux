#include "mapinc.h"
#include "../ines.h"

class EspFirmware {
public:
	EspFirmware();
	~EspFirmware();
	void tx(uint8 v);
	uint8 rx();
};

EspFirmware::EspFirmware() {
	//TODO setup network
}

EspFirmware::~EspFirmware() {
	//TODO clean network structures
}

void EspFirmware::tx(uint8 v) {
	//TODO interpret message
	FCEU_printf("EspFirmware TX %02x\n", v);
}

uint8 EspFirmware::rx() {
	//TODO figure what to do
	FCEU_printf("EspFirmware RX\n");
}

static uint8 latche, latcheinit, bus_conflict;
static uint16 addrreg0, addrreg1;
static uint8 *WRAM = NULL;
static uint32 WRAMSIZE;
static void (*WSync)(void);
static EspFirmware *esp = NULL;

static void LatchClose(void) {
	FCEU_printf("UNICORN latch close\n");
	if (WRAM)
		FCEU_gfree(WRAM);
	WRAM = NULL;
}

static DECLFW(UNICORNWrite) {
	FCEU_printf("UNICORN write %04x %02x\n", A, V);
	esp->tx(V);
}

static DECLFR(UNICORNRead) {
	FCEU_printf("UNICORN read %04x\n", A);
	return esp->rx();
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
