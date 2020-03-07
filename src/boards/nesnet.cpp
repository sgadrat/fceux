#include "mapinc.h"
#include "../ines.h"

#include "easywsclient.hpp"
#include "mongoose.h"
#include "rainbow_esp.h"

#include <algorithm>
#include <array>
#include <limits>
#include <sstream>

#undef NESNET_DEBUG
//define NESNET_DEBUG

#ifdef NESNET_DEBUG
#define UDBG(...) FCEU_printf(__VA_ARGS__)
#else
#define UDBG(...)
#endif

//////////////////////////////////////
// Mapper implementation

/* Mapper state */

static uint8 *WRAM = NULL;
static uint32 WRAMSIZE;
static EspFirmware *esp = NULL;
static bool esp_enable = true;
static bool irq_enable = true;

/* ESP interface */

static DECLFW(NESNETWrite) {
	UDBG("NESNET write %04x %02x\n", A, V);
	esp->rx(V);
}

static DECLFR(NESNETRead) {
	UDBG("NESNET read %04x\n", A);
	return esp->tx();
}

static DECLFW(NESNETWriteFlags) {
	UDBG("NESNET write %04x %02x\n", A, V);
	esp_enable = V & 0x01;
	irq_enable = V & 0x40;
}

static DECLFR(NESNETReadFlags) {
	uint8 esp_rts_flag = esp->getGpio15() ? 0x80 : 0x00;
	uint8 esp_enable_flag = esp_enable ? 0x01 : 0x00;
	uint8 irq_enable_flag = irq_enable ? 0x40 : 0x00;
	UDBG("NESNET read flags %04x => %02x\n", A, esp_rts_flag | esp_enable_flag | irq_enable_flag);
	return esp_rts_flag | esp_enable_flag | irq_enable_flag;
}

static void NESNETMapIrq(int32) {
	if (irq_enable) {
		if (esp->getGpio15()) {
			X6502_IRQBegin(FCEU_IQEXT);
		} else {
			X6502_IRQEnd(FCEU_IQEXT);
		}
	}
}

/* Mapper initialization and cleaning */

static void LatchClose(void) {
	UDBG("NESNET latch close\n");
	if (WRAM) {
		FCEU_gfree(WRAM);
	}
	WRAM = NULL;

	delete esp;
}

static void NESNETPower(void) {
	UDBG("NESNET power\n");
	setprg8r(0x10, 0x6000, 0);	// Famili BASIC (v3.0) need it (uses only 4KB), FP-BASIC uses 8KB
	setprg16(0x8000, ~1);
	setprg16(0xC000, ~0);
	setchr8(0);

	SetReadHandler(0x6000, 0x7FFF, CartBR);
	SetWriteHandler(0x6000, 0x7FFF, CartBW);
	SetReadHandler(0x8000, 0xFFFF, CartBR);

	FCEU_CheatAddRAM(WRAMSIZE >> 10, 0x6000, WRAM);

	SetWriteHandler(0x5000, 0x5000, NESNETWrite);
	SetReadHandler(0x5000, 0x5000, NESNETRead);
	SetWriteHandler(0x5001, 0x5001, NESNETWriteFlags);
	SetReadHandler(0x5001, 0x5001, NESNETReadFlags);

	esp = new BrokeStudioFirmware;
	esp_enable = true;
	irq_enable = true;
}

void NESNET_Init(CartInfo *info) {
	UDBG("NESNET init\n");
	info->Power = NESNETPower;
	info->Close = LatchClose;

	//TODO is wram really necessary?
	WRAMSIZE = 8192;
	WRAM = (uint8*)FCEU_gmalloc(WRAMSIZE);
	SetupCartPRGMapping(0x10, WRAM, WRAMSIZE, 1);
	if (info->battery) {
		info->SaveGame[0] = WRAM;
		info->SaveGameLen[0] = WRAMSIZE;
	}
	AddExState(WRAM, WRAMSIZE, 0, "WRAM");

	// Set a hook on hblank to be able periodically check if we have to send an interupt
	MapIRQHook = NESNETMapIrq;
}
