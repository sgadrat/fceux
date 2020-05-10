#include "mapinc.h"
#include "../ines.h"

#include "easywsclient.hpp"
#include "mongoose.h"
#include "nesnet_esp.h"

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
	UDBG("NESNET write flags %04x <= %02x (ignored)\n", A, V);
}

static DECLFR(NESNETReadFlags) {
	// Always return Write ready (bit 7 set) and Read ready (bit 6 unset)
	//   These are used in actual mapper because of hardware speed limitation.
	//   To be confirmed: UART transfer rate coupled with a one byte buffer on the mapper
	//   If exact, the one byte buffer could be emulated to better match hardware, for now all
	//   read/write on $5000 are directly sent to esp object.
	const uint8 flags = 0b10000000;
	UDBG("NESNET read flags %04x => %02x`n", a, flags);
	return flags;
}

static void NESNETMapIrq(int32) {
	if (irq_enable) {
		if (esp->getGpio4()) {
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

	esp = new InlFirmware;
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
