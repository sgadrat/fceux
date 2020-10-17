/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2020 Broke Studio
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// mapper 3871 - Rainbow board by Broke Studio
//
// documentation available here: https://github.com/BrokeStudio/rainbow-lib

#include "mapinc.h"
#include "../ines.h"
#include "rainbow_esp.h"

#undef RAINBOW_DEBUG
 //define RAINBOW_DEBUG

#ifdef RAINBOW_DEBUG
#define UDBG(...) FCEU_printf(__VA_ARGS__)
#else
#define UDBG(...)
#endif

#define MAPPER_VERSION		0b00100000

#define MIRR_VERTICAL       0b00 // VRAM
#define MIRR_HORIZONTAL     0b01 // VRAM
#define MIRR_ONE_SCREEN     0b10 // VRAM [+ CHR-RAM]
#define MIRR_FOUR_SCREEN    0b11 // CHR-RAM

#define CHR_TYPE_ROM        0b0  // CHR-ROM
#define CHR_TYPE_RAM        0b1  // CHR-RAM

#define CHR_MODE_1K         0b00 // 1K mode
#define CHR_MODE_2K         0b01 // 2K mode
#define CHR_MODE_4K         0b10 // 4K mode
#define CHR_MODE_8K         0b11 // 8K mode

#define PRG_MODE_16K_8K     0b0 // 16K + 8K + 8K fixed
#define PRG_MODE_8K_8K_8K   0b1 // 8K + 8K + 8K fixed

static uint8 prg[3], chr[8], wram_bank;
static uint8 prg_mode, chr_mode_chip, chr_mode;
static uint8 mirr_mode, nt_set;

static uint8 *WRAM = NULL;
const uint32 WRAMSIZE = 32 * 1024;

static uint8 *DUMMY_CHRRAM = NULL;
const uint32 DUMMY_CHRRAMSIZE = 4 * 1024;

static uint8 *DUMMY_CHRROM = NULL;
const uint32 DUMMY_CHRROMSIZE = 512 * 1024;

static uint8 *CHRRAM = NULL;
const uint32 CHRRAMSIZE = 32 * 1024;

extern uint8 *ExtraNTARAM;

extern uint8 IRQCount, IRQLatch, IRQa;
extern uint8 IRQReload;

static uint8 flash_mode[2];
static uint8 flash_sequence[2];
static uint8 flash_id[2];

static uint8 *PRG_FLASHROM = NULL;
const uint32 PRG_FLASHROMSIZE = 512 * 1024;

static uint8 *CHR_FLASHROM = NULL;
const uint32 CHR_FLASHROMSIZE = 512 * 1024;

static SFORMAT FlashRegs[] =
{
	{ &flash_mode, 2, "FMOD" },
	{ &flash_sequence, 2, "FSEQ" },
	{ &flash_id, 2, "FMID" },
	{ 0 }
};

static SFORMAT RainbowStateRegs[] =
{
	{ prg, 3, "PRG" },
	{ chr, 8, "CHR" },
	{ &IRQReload, 1, "IRQR" },
	{ &IRQCount, 1, "IRQC" },
	{ &IRQLatch, 1, "IRQL" },
	{ &IRQa, 1, "IRQA" },
	{ 0 }
};

static void(*sfun[3]) (void);
static uint8 vpsg1[8];
static uint8 vpsg2[4];
static int32 cvbc[3];
static int32 vcount[3];
static int32 dcount[2];

static SFORMAT SStateRegs[] =
{
	{ vpsg1, 8, "PSG1" },
	{ vpsg2, 4, "PSG2" },
	{ 0 }
};

enum CHIP_TYPE
{
	prg_chip = 0,
	chr_chip
};

// ESP interface

static EspFirmware *esp = NULL;
static bool esp_enable = true;
static bool irq_enable = true;

static DECLFR(RainbowEspReadFlags) {
	uint8 esp_rts_flag = esp->getGpio4() ? 0x80 : 0x00;
	uint8 esp_enable_flag = esp_enable ? 0x01 : 0x00;
	uint8 irq_enable_flag = irq_enable ? 0x40 : 0x00;
	UDBG("RAINBOW read flags %04x => %02x\n", A, esp_rts_flag | esp_enable_flag | irq_enable_flag);
	return esp_rts_flag | esp_enable_flag | irq_enable_flag;
}

static void RainbowEspMapIrq(int32) {
	if (irq_enable)
	{
		if (esp->getGpio4())
		{
			X6502_IRQBegin(FCEU_IQEXT);
		}
		else
		{
			X6502_IRQEnd(FCEU_IQEXT);
		}
	}
}

// Mapper

static void Sync(void) {
	static uint8 *address;
	uint32 start;
	uint32 offset;
	uint8 cart_chr_map;

	if (prg_mode == PRG_MODE_16K_8K)
	{
		if (prg[0] & 0x80)
		{
			setprg8r(0x10, 0x8000, prg[0] & 0x03);
			setprg8r(0x10, 0xa000, (prg[0] + 1) & 0x03);
		}
		else
			setprg16r(0x11, 0x8000, prg[0]);

		if (prg[2] & 0x80)
			setprg8r(0x10, 0xc000, prg[2] & 0x03);
		else
			setprg8r(0x11, 0xc000, prg[2] & 0x3f);
	}

	if (prg_mode == PRG_MODE_8K_8K_8K)
	{
		if (prg[0] & 0x80)
			setprg8r(0x10, 0x8000, prg[0] & 0x03);
		else
			setprg8r(0x11, 0x8000, prg[0] & 0x3f);

		if (prg[1] & 0x80)
			setprg8r(0x10, 0xa000, prg[1] & 0x03);
		else
			setprg8r(0x11, 0xa000, prg[1] & 0x3f);

		if (prg[2] & 0x80)
			setprg8r(0x10, 0xc000, prg[2] & 0x03);
		else
			setprg8r(0x11, 0xc000, prg[2] & 0x3f);
	}

	setprg8r(0x11, 0xe000, ~0);

	if (wram_bank & 0x80)
		setprg8r(0x10, 0x6000, wram_bank & 0x03);
	else
		setprg8r(0x11, 0x6000, wram_bank & 0x7f);

	cart_chr_map = chr_mode_chip + 0x10;
	switch (chr_mode)
	{
	case CHR_MODE_1K:
		for (uint8 i = 0; i < 8; i++) {
			if ((i < 4) | (chr_mode_chip == CHR_TYPE_RAM))
				setchr1r(cart_chr_map, i << 10, chr[i]);
			else
				setchr1r(cart_chr_map, i << 10, chr[i] + 256);
		}
		break;
	case CHR_MODE_2K:
		setchr2r(cart_chr_map, 0x0000, chr[0]);
		setchr2r(cart_chr_map, 0x0800, chr[1]);
		setchr2r(cart_chr_map, 0x1000, chr[2]);
		setchr2r(cart_chr_map, 0x1800, chr[3]);
		break;
	case CHR_MODE_4K:
		setchr4r(cart_chr_map, 0x0000, chr[0]);
		setchr4r(cart_chr_map, 0x1000, chr[1]);
		break;
	case CHR_MODE_8K:
		setchr8r(cart_chr_map, chr[0]);
		break;
	}

	switch (mirr_mode)
	{
	case MIRR_VERTICAL:
		setmirror(MI_V);
		break;
	case MIRR_HORIZONTAL:
		setmirror(MI_H);
		break;
	case MIRR_ONE_SCREEN:
		FCEUPPU_LineUpdate();
		switch (nt_set)
		{
		case 0: address = NTARAM; break;
		case 1: address = NTARAM + 0x400; break;
		case 2: address = ExtraNTARAM; break;
		case 3: address = ExtraNTARAM + 0x400; break;
		}
		vnapage[0] = vnapage[1] = vnapage[2] = vnapage[3] = address;
		PPUNTARAM = 0x0F;
		break;
	case MIRR_FOUR_SCREEN:
		FCEUPPU_LineUpdate();
		if (CHRRAM != NULL)
		{
			for (int i = 0; i < 4; i++)
			{
				uint8 t = nt_set ^ 0x03;
				start = (16 + t * 4) * 1024;
				offset = i * 0x400;
				vnapage[i] = CHRRAM + start + offset;
			}
			PPUNTARAM = 0x0F;
		}
		else
		{
			for (int i = 0; i < 4; i++)
			{
				offset = i * 0x400;
				vnapage[i] = DUMMY_CHRRAM + offset;
			}
		}
	}
}

static DECLFW(RainbowSW) {
	if (A >= 0x5800 && A <= 0x5802)
	{
		vpsg1[A & 3] = V;
		if (sfun[0])
			sfun[0]();
	}
	else if (A >= 0x5803 && A <= 0x5805)
	{
		vpsg1[4 | ((A - 3) & 3)] = V;
		if (sfun[1])
			sfun[1]();
	}
	else if (A >= 0x5C00 && A <= 0x5C02)
	{
		vpsg2[A & 3] = V;
		if (sfun[2])
			sfun[2]();
	}
}

static DECLFR(RainbowRead) {
	switch (A)
	{
	case 0x5000:
		return esp->tx();
	case 0x5001:
	{
		uint8 esp_rts_flag = esp->getGpio4() ? 0x80 : 0x00;
		uint8 esp_enable_flag = esp_enable ? 0x01 : 0x00;
		uint8 irq_enable_flag = irq_enable ? 0x40 : 0x00;
		return esp_rts_flag | esp_enable_flag | irq_enable_flag;
	}
	case 0x5006:
		return (nt_set << 6) | (mirr_mode << 4) | (chr_mode_chip << 3) | (chr_mode << 1) | prg_mode;
	case 0x5C03:
		return MAPPER_VERSION;
	default:
		return 0;
	}
}

static DECLFW(RainbowWrite) {
	switch (A)
	{
	case 0x5000: esp->rx(V); break;
	case 0x5001:
		esp_enable = V & 0x01;
		irq_enable = V & 0x40;
		break;
	case 0x5002: prg[0] = V; Sync(); break;
	case 0x5003: prg[1] = V & 0x3f; Sync(); break;
	case 0x5004: prg[2] = V & 0x3f; Sync(); break;
	case 0x5005: wram_bank = V; Sync(); break;
	case 0x5006:
		prg_mode = V & 0x01;
		chr_mode_chip = (V & 0x08) >> 3;
		chr_mode = (V & 0x06) >> 1;
		mirr_mode = (V & 0x30) >> 4;
		nt_set = (V & 0xC0) >> 6;
		Sync();
		break;

	case 0x5400: chr[0] = V; Sync(); break;
	case 0x5401: chr[1] = V; Sync(); break;
	case 0x5402: chr[2] = V; Sync(); break;
	case 0x5403: chr[3] = V; Sync(); break;
	case 0x5404: chr[4] = V; Sync(); break;
	case 0x5405: chr[5] = V; Sync(); break;
	case 0x5406: chr[6] = V; Sync(); break;
	case 0x5407: chr[7] = V; Sync(); break;

	case 0x5C04: IRQLatch = V; break;
	case 0x5C05: IRQReload = 1; break;
	case 0x5C06:
		X6502_IRQEnd(FCEU_IQEXT);
		IRQa = 0;
		break;
	case 0x5C07:
		IRQa = 1;
		break;
	}
}

static void ClockRainbowCounter(void) {
	int count = IRQCount;
	if (!count || IRQReload)
	{
		IRQCount = IRQLatch;
		IRQReload = 0;
	}
	else
		IRQCount--;

	if ((count | 1) && !IRQCount)
	{
		if (IRQa)
			X6502_IRQBegin(FCEU_IQEXT);
	}
}

static void Rainbowhb() {
	ClockRainbowCounter();
}

uint8 FASTCALL RainbowPPURead(uint32 A) {
	// if CHR-RAM, check if CHR-RAM exists, if not return data bus cache
	if (chr_mode_chip == CHR_TYPE_RAM && CHRRAM == NULL)
	{
		if (PPU_hook) PPU_hook(A);
		return X.DB;
	}

	// if CHR-ROM, check if CHR-ROM exists, if not return data bus cache
	if (chr_mode_chip == CHR_TYPE_ROM && CHR_FLASHROM == NULL)
	{
		if (PPU_hook) PPU_hook(A);
		return X.DB;
	}

	return FFCEUX_PPURead_Default(A);
}

uint8 RainbowFlashID(uint32 A) {
	// Software ID mode is undefined by the datasheet for all but the lowest 2 addressable bytes,
	// but some tests of the chip currently being used found it repeats in 512-byte patterns.
	// http://forums.nesdev.com/viewtopic.php?p=178728#p178728

	uint32 aid = A & 0x1FF;
	switch (aid)
	{
	case 0:  return 0xBF;
	case 1:  return 0xB7;
	default: return 0xFF;
	}
}

uint8 FASTCALL RainbowFlashChrID(uint32 A) {
	return RainbowFlashID(A);
}

static DECLFR(RainbowFlashPrgID)
{
	return RainbowFlashID(A);
}

void RainbowFlashIDEnter(CHIP_TYPE chip)
{
	switch (chip)
	{
	case CHIP_TYPE::prg_chip:
		if (flash_id[chip])
			return;
		flash_id[chip] = 1;
		SetReadHandler(0x8000, 0xFFFF, RainbowFlashPrgID);
		break;
	case CHIP_TYPE::chr_chip:
		if (flash_id[chip])
			return;
		flash_id[chip] = 1;
		FFCEUX_PPURead = RainbowFlashChrID;
		break;
	default:
		return;
	}
}

void RainbowFlashIDExit(CHIP_TYPE chip)
{
	switch (chip)
	{
	case CHIP_TYPE::prg_chip:
		if (!flash_id[chip])
			return;
		flash_id[chip] = 0;
		SetReadHandler(0x8000, 0xFFFF, CartBR);
		break;
	case CHIP_TYPE::chr_chip:
		if (!flash_id[chip])
			return;
		flash_id[chip] = 0;
		FFCEUX_PPURead = RainbowPPURead;
		break;
	default:
		return;
	}
}

void RainbowFlash(CHIP_TYPE chip, uint32 flash_addr, uint8 V) {

	uint32 command_addr = flash_addr & 0x7FFF;

	enum
	{
		flash_mode_READY = 0,
		flash_mode_COMMAND,
		flash_mode_BYTE_WRITE,
		flash_mode_ERASE,
	};

	switch (flash_mode[chip])
	{
	default:
	case flash_mode_READY:
		if (command_addr == 0x5555 && V == 0xAA)
		{
			flash_mode[chip] = flash_mode_COMMAND;
			flash_sequence[chip] = 0;
		}
		else if (V == 0xF0)
		{
			RainbowFlashIDExit(chip);
		}
		break;
	case flash_mode_COMMAND:
		if (flash_sequence[chip] == 0)
		{
			if (command_addr == 0x2AAA && V == 0x55)
			{
				flash_sequence[chip] = 1;
			}
			else
			{
				flash_mode[chip] = flash_mode_READY;
			}
		}
		else if (flash_sequence[chip] == 1)
		{
			if (command_addr == 0x5555)
			{
				flash_sequence[chip] = 0;
				switch (V)
				{
				default:   flash_mode[chip] = flash_mode_READY; break;
				case 0xA0: flash_mode[chip] = flash_mode_BYTE_WRITE; break;
				case 0x80: flash_mode[chip] = flash_mode_ERASE; break;
				case 0x90: RainbowFlashIDEnter(chip); flash_mode[chip] = flash_mode_READY; break;
				case 0xF0: RainbowFlashIDExit(chip); flash_mode[chip] = flash_mode_READY; break;
				}
			}
			else
				flash_mode[chip] = flash_mode_READY;
		}
		else
			flash_mode[chip] = flash_mode_READY; // should be unreachable
		break;
	case flash_mode_BYTE_WRITE:
		if (chip == CHIP_TYPE::prg_chip)
		{
			PRG_FLASHROM[flash_addr] &= V;
		}
		else if (chip == CHIP_TYPE::chr_chip)
		{
			CHR_FLASHROM[flash_addr] &= V;
		}
		flash_mode[chip] = flash_mode_READY;
		break;
	case flash_mode_ERASE:
		if (flash_sequence[chip] == 0)
		{
			if (command_addr == 0x5555 && V == 0xAA)
				flash_sequence[chip] = 1;
			else
				flash_mode[chip] = flash_mode_READY;
		}
		else if (flash_sequence[chip] == 1)
		{
			if (command_addr == 0x2AAA && V == 0x55)
				flash_sequence[chip] = 2;
			else
				flash_mode[chip] = flash_mode_READY;
		}
		else if (flash_sequence[chip] == 2)
		{
			if (command_addr == 0x5555 && V == 0x10) // erase chip
			{
				if (chip == CHIP_TYPE::prg_chip)
				{
					memset(PRG_FLASHROM, 0xFF, PRG_FLASHROMSIZE);
				}
				else if (chip == CHIP_TYPE::chr_chip)
				{
					memset(CHR_FLASHROM, 0xFF, CHR_FLASHROMSIZE);
				}
			}
			else if (V == 0x30) // erase 4k sector
			{
				uint32 sector = flash_addr & 0x7F000;
				if (chip == CHIP_TYPE::prg_chip)
				{
					memset(PRG_FLASHROM + sector, 0xFF, 1024 * 4);
				}
				else if (chip == CHIP_TYPE::chr_chip)
				{
					memset(CHR_FLASHROM + sector, 0xFF, 1024 * 4);
				}
			}
			flash_mode[chip] = flash_mode_READY;
		}
		else
			flash_mode[chip] = flash_mode_READY; // should be unreachable
		break;
	}
}

static DECLFW(RainbowPrgFlash) {
	if (A < 0x8000 || A > 0xFFFF)
		return;

	uint32 flash_addr = A;
	switch (prg_mode)
	{
	case PRG_MODE_8K_8K_8K:
		flash_addr &= 0x1FFF;
		if ((A >= 0x8000) & (A <= 0x9FFF))
		{
			flash_addr |= (prg[0] & 0x3F) << 13;
		}
		else if ((A >= 0xA000) & (A <= 0xBFFF))
		{
			flash_addr |= (prg[1] & 0x3F) << 13;
		}
		else if ((A >= 0xC000) & (A <= 0xDFFF))
		{
			flash_addr |= (prg[2] & 0x3F) << 13;
		}
		break;
	case PRG_MODE_16K_8K:
		if ((A >= 0x8000) & (A <= 0xBFFF))
		{
			flash_addr &= 0x3FFF;
			flash_addr |= (prg[0] & 0x3F) << 14;
		}
		else if ((A >= 0xC000) & (A <= 0xDFFF))
		{
			flash_addr &= 0x1FFF;
			flash_addr |= (prg[2] & 0x3F) << 13;
		}
		break;
	default:
		return;
	}
	RainbowFlash(CHIP_TYPE::prg_chip, flash_addr, V);
}

static void RainbowPPUWrite(uint32 A, uint8 V) {

	// if CHR-RAM, check if CHR-RAM exists, if not return
	if (chr_mode_chip == CHR_TYPE_RAM && CHRRAM == NULL)
		return;

	// if CHR-ROM, check if CHR-ROM exists, if not return
	if (chr_mode_chip == CHR_TYPE_ROM && CHR_FLASHROM == NULL)
		return;
	else
	{
		uint32 flash_addr = A;
		if (A < 0x2000)
		{
			switch (chr_mode)
			{
			case CHR_MODE_1K:
				flash_addr &= 0x3FF;
				flash_addr |= chr[A >> 10] << 10;
				break;
			case CHR_MODE_2K:
				flash_addr &= 0x7FF;
				flash_addr |= chr[A >> 11] << 11;
				break;
			case CHR_MODE_4K:
				flash_addr &= 0xFFF;
				flash_addr |= chr[A >> 12] << 12;
				break;
			case CHR_MODE_8K:
			default:
				flash_addr &= 0x1FFF;
				flash_addr |= chr[0] << 13;
				break;
			}
			RainbowFlash(CHIP_TYPE::chr_chip, flash_addr, V);
		}
	}
	FFCEUX_PPUWrite_Default(A, V);
}

static void RainbowPower(void) {

	// mapper
	IRQCount = IRQLatch = IRQa = 0;
	prg_mode = PRG_MODE_16K_8K;
	Sync();
	SetReadHandler(0x6000, 0xFFFF, CartBR);
	SetWriteHandler(0x6000, 0x7FFF, CartBW);
	FCEU_CheatAddRAM(0x10, 0x6000, WRAM);

	// mapper registers (writes)
	SetWriteHandler(0x5000, 0x5007, RainbowWrite);
	SetWriteHandler(0x5400, 0x5407, RainbowWrite);
	SetWriteHandler(0x5806, 0x5807, RainbowWrite);
	SetWriteHandler(0x5C04, 0x5C07, RainbowWrite);

	// audio expansion registers (writes)
	SetWriteHandler(0x5800, 0x5805, RainbowSW);
	SetWriteHandler(0x5C00, 0x5C02, RainbowSW);

	// mapper registers (reads)
	SetReadHandler(0x5000, 0x5C07, RainbowRead);

	// self-flashing
	flash_mode[CHIP_TYPE::prg_chip] = 0;
	flash_mode[CHIP_TYPE::chr_chip] = 0;
	flash_sequence[CHIP_TYPE::prg_chip] = 0;
	flash_sequence[CHIP_TYPE::chr_chip] = 0;
	flash_id[CHIP_TYPE::prg_chip] = false;
	flash_id[CHIP_TYPE::chr_chip] = false;
	SetWriteHandler(0x8000, 0xFFFF, RainbowPrgFlash);

	// fill WRAM/CHRRAM/DUMMY_CHRRAM/DUMMY_CHRROM with random values
	if(WRAM)
		FCEU_MemoryRand(WRAM, WRAMSIZE, false);

	if(CHRRAM)
		FCEU_MemoryRand(CHRRAM, CHRRAMSIZE, false);

	if(DUMMY_CHRRAM)
		FCEU_MemoryRand(DUMMY_CHRRAM, DUMMY_CHRRAMSIZE, false);

	if (DUMMY_CHRROM)
		FCEU_MemoryRand(DUMMY_CHRROM, DUMMY_CHRROMSIZE, false);

	// ESP firmware
	esp = new BrokeStudioFirmware;
	esp_enable = true;
	irq_enable = true;
}

static void RainbowClose(void)
{
	if (WRAM)
	{
		FCEU_gfree(WRAM);
		WRAM = NULL;
	}

	if (CHRRAM)
		ExtraNTARAM = NULL;

	if (DUMMY_CHRRAM)
	{
		FCEU_gfree(DUMMY_CHRRAM);
		DUMMY_CHRRAM = NULL;
		ExtraNTARAM = NULL;
	}

	if (PRG_FLASHROM)
	{
		FCEU_gfree(PRG_FLASHROM);
		PRG_FLASHROM = NULL;
	}

	if (CHR_FLASHROM)
	{
		FCEU_gfree(CHR_FLASHROM);
		CHR_FLASHROM = NULL;
	}

	if (esp)
	{
		delete esp;
		esp = NULL;
	}
}

static void StateRestore(int version) {
	Sync();
}

// audio expansion

static void DoSQV1(void);
static void DoSQV2(void);
static void DoSawV(void);

static INLINE void DoSQV(int x) {
	int32 V;
	int32 amp = (((vpsg1[x << 2] & 15) << 8) * 6 / 8) >> 4;
	int32 start, end;

	start = cvbc[x];
	end = (SOUNDTS << 16) / soundtsinc;
	if (end <= start)
		return;
	cvbc[x] = end;

	if (vpsg1[(x << 2) | 0x2] & 0x80)
	{
		if (vpsg1[x << 2] & 0x80)
		{
			for (V = start; V < end; V++)
				Wave[V >> 4] += amp;
		}
		else
		{
			int32 thresh = (vpsg1[x << 2] >> 4) & 7;
			int32 freq = ((vpsg1[(x << 2) | 0x1] | ((vpsg1[(x << 2) | 0x2] & 15) << 8)) + 1) << 17;
			for (V = start; V < end; V++) {
				if (dcount[x] > thresh)
					Wave[V >> 4] += amp;
				vcount[x] -= nesincsize;
				while (vcount[x] <= 0) {
					vcount[x] += freq;
					dcount[x] = (dcount[x] + 1) & 15;
				}
			}
		}
	}
}

static void DoSQV1(void) {
	DoSQV(0);
}

static void DoSQV2(void) {
	DoSQV(1);
}

static void DoSawV(void) {
	int V;
	int32 start, end;

	start = cvbc[2];
	end = (SOUNDTS << 16) / soundtsinc;
	if (end <= start)
		return;
	cvbc[2] = end;

	if (vpsg2[2] & 0x80)
	{
		static int32 saw1phaseacc = 0;
		uint32 freq3;
		static uint8 b3 = 0;
		static int32 phaseacc = 0;
		static uint32 duff = 0;

		freq3 = (vpsg2[1] + ((vpsg2[2] & 15) << 8) + 1);

		for (V = start; V < end; V++) {
			saw1phaseacc -= nesincsize;
			if (saw1phaseacc <= 0)
			{
				int32 t;
rea:
				t = freq3;
				t <<= 18;
				saw1phaseacc += t;
				phaseacc += vpsg2[0] & 0x3f;
				b3++;
				if (b3 == 7)
				{
					b3 = 0;
					phaseacc = 0;
				}
				if (saw1phaseacc <= 0)
					goto rea;
				duff = (((phaseacc >> 3) & 0x1f) << 4) * 6 / 8;
			}
			Wave[V >> 4] += duff;
		}
	}
}

static INLINE void DoSQVHQ(int x) {
	int32 V;
	int32 amp = ((vpsg1[x << 2] & 15) << 8) * 6 / 8;

	if (vpsg1[(x << 2) | 0x2] & 0x80)
	{
		if (vpsg1[x << 2] & 0x80)
		{
			for (V = cvbc[x]; V < (int)SOUNDTS; V++)
				WaveHi[V] += amp;
		}
		else
		{
			int32 thresh = (vpsg1[x << 2] >> 4) & 7;
			for (V = cvbc[x]; V < (int)SOUNDTS; V++) {
				if (dcount[x] > thresh)
					WaveHi[V] += amp;
				vcount[x]--;
				if (vcount[x] <= 0)
				{
					vcount[x] = (vpsg1[(x << 2) | 0x1] | ((vpsg1[(x << 2) | 0x2] & 15) << 8)) + 1;
					dcount[x] = (dcount[x] + 1) & 15;
				}
			}
		}
	}
	cvbc[x] = SOUNDTS;
}

static void DoSQV1HQ(void) {
	DoSQVHQ(0);
}

static void DoSQV2HQ(void) {
	DoSQVHQ(1);
}

static void DoSawVHQ(void) {
	static uint8 b3 = 0;
	static int32 phaseacc = 0;
	int32 V;

	if (vpsg2[2] & 0x80)
	{
		for (V = cvbc[2]; V < (int)SOUNDTS; V++) {
			WaveHi[V] += (((phaseacc >> 3) & 0x1f) << 8) * 6 / 8;
			vcount[2]--;
			if (vcount[2] <= 0)
			{
				vcount[2] = (vpsg2[1] + ((vpsg2[2] & 15) << 8) + 1) << 1;
				phaseacc += vpsg2[0] & 0x3f;
				b3++;
				if (b3 == 7) {
					b3 = 0;
					phaseacc = 0;
				}
			}
		}
	}
	cvbc[2] = SOUNDTS;
}

void RainbowSound(int Count) {
	int x;

	DoSQV1();
	DoSQV2();
	DoSawV();
	for (x = 0; x < 3; x++)
		cvbc[x] = Count;
}

void RainbowSoundHQ(void) {
	DoSQV1HQ();
	DoSQV2HQ();
	DoSawVHQ();
}

void RainbowSyncHQ(int32 ts) {
	int x;
	for (x = 0; x < 3; x++) cvbc[x] = ts;
}

static void RainbowESI(void) {
	GameExpSound.RChange = RainbowESI;
	GameExpSound.Fill = RainbowSound;
	GameExpSound.HiFill = RainbowSoundHQ;
	GameExpSound.HiSync = RainbowSyncHQ;

	memset(cvbc, 0, sizeof(cvbc));
	memset(vcount, 0, sizeof(vcount));
	memset(dcount, 0, sizeof(dcount));
	if (FSettings.SndRate)
	{
		if (FSettings.soundq >= 1)
		{
			sfun[0] = DoSQV1HQ;
			sfun[1] = DoSQV2HQ;
			sfun[2] = DoSawVHQ;
		}
		else
		{
			sfun[0] = DoSQV1;
			sfun[1] = DoSQV2;
			sfun[2] = DoSawV;
		}
	}
	else
		memset(sfun, 0, sizeof(sfun));
	AddExState(&SStateRegs, ~0, 0, 0);
}

#if 0
// Let's disable NSF support for now since I don't have an NSF ROM file to test it yet.

// NSF Init

void NSFRainbow_Init(void) {
	RainbowESI();
	SetWriteHandler(0x8000, 0xbfff, RainbowSW);
}
#endif

// mapper init

void RAINBOW_Init(CartInfo *info) {
	int save_game_index = 0;
	info->Power = RainbowPower;
	info->Close = RainbowClose;

	GameHBIRQHook = Rainbowhb;
	RainbowESI();
	GameStateRestore = StateRestore;

	// WRAM
	if (info->wram_size != 0)
	{
		WRAM = (uint8*)FCEU_gmalloc(WRAMSIZE);
		SetupCartPRGMapping(0x10, WRAM, WRAMSIZE, 1);
		AddExState(WRAM, WRAMSIZE, 0, "WRAM");

		if (info->battery)
		{
			info->SaveGame[save_game_index] = WRAM;
			info->SaveGameLen[save_game_index] = WRAMSIZE;
			save_game_index++;
		}

	}

	// PRG FLASH ROM
	PRG_FLASHROM = (uint8*)FCEU_gmalloc(PRG_FLASHROMSIZE);
	AddExState(PRG_FLASHROM, PRG_FLASHROMSIZE, 0, "PFROM");
	if (info->battery)
	{
		info->SaveGame[save_game_index] = PRG_FLASHROM;
		info->SaveGameLen[save_game_index] = PRG_FLASHROMSIZE;
		save_game_index++;
	}

	// copy PRG ROM into PRG_FLASHROM, use it instead of PRG ROM
	const uint32 PRGSIZE = ROM_size * 16 * 1024;
	for (uint32 w = 0, r = 0; w < PRG_FLASHROMSIZE; ++w)
	{
		PRG_FLASHROM[w] = ROM[r];
		++r;
		if (r >= PRGSIZE)
			r = 0;
	}
	SetupCartPRGMapping(0x11, PRG_FLASHROM, PRG_FLASHROMSIZE, 0);

	// CHR-RAM
	if (info->vram_size != 0)
	{
		CHRRAM = (uint8*)FCEU_gmalloc(CHRRAMSIZE);
		SetupCartCHRMapping(0x11, CHRRAM, CHRRAMSIZE, 1);
		AddExState(CHRRAM, CHRRAMSIZE, 0, "CRAM");
		ExtraNTARAM = CHRRAM + 30 * 1024;
		AddExState(ExtraNTARAM, 2048, 0, "EXNR");
	}
	else
	{
		// create dummy CHR-RAM to avoid crash when trying to use CHR-RAM for nametables
		// when no CHR-RAM is specified in the ROM header
		DUMMY_CHRRAM = (uint8*)FCEU_gmalloc(DUMMY_CHRRAMSIZE);
		ExtraNTARAM = DUMMY_CHRRAM;
		AddExState(ExtraNTARAM, DUMMY_CHRRAMSIZE, 0, "EXNR");
	}

	// CHR FLASHROM
	if (VROM_size != 0)
	{
		CHR_FLASHROM = (uint8*)FCEU_gmalloc(CHR_FLASHROMSIZE);
		AddExState(CHR_FLASHROM, CHR_FLASHROMSIZE, 0, "CFROM");
		if (info->battery)
		{
			info->SaveGame[save_game_index] = CHR_FLASHROM;
			info->SaveGameLen[save_game_index] = CHR_FLASHROMSIZE;
			save_game_index++;
		}

		// copy CHR ROM into CHR_FLASHROM, use it instead of CHR ROM
		const uint32 CHRSIZE = VROM_size * 8 * 1024;
		for (uint32 w = 0, r = 0; w < CHR_FLASHROMSIZE; ++w)
		{
			CHR_FLASHROM[w] = VROM[r];
			++r;
			if (r >= CHRSIZE)
				r = 0;
		}
		SetupCartCHRMapping(0x10, CHR_FLASHROM, CHR_FLASHROMSIZE, 0);

		FFCEUX_PPUWrite = RainbowPPUWrite;
		FFCEUX_PPURead = RainbowPPURead;
	}
	else
	{
		// create dummy CHR-ROM to avoid crash when trying to use CHR-ROM for pattern tables
		// when no CHR-ROM is specified in the ROM header
		DUMMY_CHRROM = (uint8*)FCEU_gmalloc(DUMMY_CHRROMSIZE);
		SetupCartCHRMapping(0x10, DUMMY_CHRROM, DUMMY_CHRROMSIZE, 0);
	}

	AddExState(&FlashRegs, ~0, 0, 0);
	AddExState(&RainbowStateRegs, ~0, 0, 0);

	// set a hook on hblank to be able periodically check if we have to send an interupt
	MapIRQHook = RainbowEspMapIrq;
}
