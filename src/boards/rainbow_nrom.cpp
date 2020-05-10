#include "mapinc.h"
#include "../ines.h"

#include "easywsclient.hpp"
#include "mongoose.h"
#include "rainbow_esp.h"

#include <algorithm>
#include <array>
#include <limits>
#include <sstream>

#undef RAINBOW_DEBUG
//define RAINBOW_DEBUG

#ifdef RAINBOW_DEBUG
#define UDBG(...) FCEU_printf(__VA_ARGS__)
#else
#define UDBG(...)
#endif

//////////////////////////////////////
// Mapper implementation

namespace
{
/* Flash write unlock sequence logic */

enum class flash_write_mode_t : uint8
{
	WRITE_DISABLED,
	ERASE_SECTOR,
	WRITE_BYTE,
};

class FlashSequenceCounter
{
public:
	FlashSequenceCounter(std::array<uint32, 5> const &sequence_addresses);
	void reset();
	flash_write_mode_t addWrite(uint32 address, uint8 value);

private:
	std::array<uint32, 5> sequence_addresses;
	uint8 sequence_position = 0;
};

FlashSequenceCounter::FlashSequenceCounter(std::array<uint32, 5> const &sequence_addresses)
		: sequence_addresses(sequence_addresses)
{
}

void FlashSequenceCounter::reset()
{
	this->sequence_position = 0;
}

flash_write_mode_t FlashSequenceCounter::addWrite(uint32 address, uint8 value)
{
	uint8 const sequence_values[5] = {0xaa, 0x55, 0x80, 0xaa, 0x55};

	if (address == this->sequence_addresses[this->sequence_position] && value == sequence_values[this->sequence_position])
	{
		++this->sequence_position;
		if (this->sequence_position == 5)
		{
			return flash_write_mode_t::ERASE_SECTOR;
		}
		else
		{
			return flash_write_mode_t::WRITE_DISABLED;
		}
	}
	else if (this->sequence_position == 2 && value == 0xa0)
	{
		return flash_write_mode_t::WRITE_BYTE;
	}
	else
	{
		this->reset();
		return flash_write_mode_t::WRITE_DISABLED;
	}
}

} // namespace

/* Mapper state */

static uint8 *WRAM = NULL;
static uint32 WRAMSIZE;
static uint8 *flash_prg = NULL;
static bool *flash_prg_written = NULL;
static uint32 const flash_prg_size = 32 * 1024;
static uint8 *flash_chr = NULL;
static bool *flash_chr_written = NULL;
static uint32 const flash_chr_size = 8 * 1024;
static EspFirmware *esp = NULL;
static bool esp_enable = true;
static bool irq_enable = true;
static FlashSequenceCounter flash_prg_sequence_counter({0xd555, 0xaaaa, 0xd555, 0xd555, 0xaaaa});
static flash_write_mode_t flash_prg_write_mode = flash_write_mode_t::WRITE_DISABLED;
static FlashSequenceCounter flash_chr_sequence_counter({0x1555, 0x0aaa, 0x1555, 0x1555, 0x0aaa});
static flash_write_mode_t flash_chr_write_mode = flash_write_mode_t::WRITE_DISABLED;

/* ESP interface */

static DECLFW(RAINBOW_NROMWrite)
{
	UDBG("RAINBOW write %04x %02x\n", A, V);
	esp->rx(V);
}

static DECLFR(RAINBOW_NROMRead)
{
	UDBG("RAINBOW read %04x\n", A);
	return esp->tx();
}

static DECLFW(RAINBOW_NROMWriteFlags)
{
	UDBG("RAINBOW write %04x %02x\n", A, V);
	esp_enable = V & 0x01;
	irq_enable = V & 0x40;
}

static DECLFR(RAINBOW_NROMReadFlags)
{
	uint8 esp_rts_flag = esp->getGpio4() ? 0x80 : 0x00;
	uint8 esp_enable_flag = esp_enable ? 0x01 : 0x00;
	uint8 irq_enable_flag = irq_enable ? 0x40 : 0x00;
	UDBG("RAINBOW read flags %04x => %02x\n", A, esp_rts_flag | esp_enable_flag | irq_enable_flag);
	return esp_rts_flag | esp_enable_flag | irq_enable_flag;
}

static void RAINBOW_NROMMapIrq(int32)
{
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

/* Flash memory writing logic */

DECLFW(RAINBOW_NROMWriteFlash)
{
	assert(0x8000 <= A && A <= 0xffff);
	UDBG("RAINBOW write flash %04x => %02x\n", A, V);
	bool reset_flash_mode = false;
	switch (flash_prg_write_mode)
	{
	case flash_write_mode_t::WRITE_DISABLED:
		flash_prg_write_mode = flash_prg_sequence_counter.addWrite(A, V);
		break;
	case flash_write_mode_t::ERASE_SECTOR:
		UDBG("RAINBOW erase sector %04x %02x\n", A, V);
		if (A == 0x8000 || A == 0x9000 || A == 0xa000 || A == 0xb000 || A == 0xc000 || A == 0xd000 || A == 0xe000 || A == 0xf000)
		{
			::memset(flash_prg + (A - 0x8000), 0xff, 0x1000);
			for (uint32 i = A; i < A + 0x1000; ++i)
			{
				flash_prg_written[i - 0x8000] = true;
			}
			for (uint32_t c = 0; c < 0x1000; ++c)
			{
				UDBG("%02x", flash_prg[(A - 0x8000) + c]);
			}
			UDBG("\n");
		}
		reset_flash_mode = true;
		break;
	case flash_write_mode_t::WRITE_BYTE:
		UDBG("RAINBOW write byte %04x %02x (previous=%02x)\n", A, V, flash_prg[A - 0x8000]);
		flash_prg[A - 0x8000] &= V;
		flash_prg_written[A - 0x8000] = true;
		reset_flash_mode = true;
		break;
	};

	if (reset_flash_mode)
	{
		flash_prg_sequence_counter.reset();
		flash_prg_write_mode = flash_write_mode_t::WRITE_DISABLED;
	}
}

DECLFR(RAINBOW_NROMReadFlash)
{
	assert(0x8000 <= A && A <= 0xffff);
	//UDBG("RAINBOW read flash %04x\n", A);
	if (flash_prg_written[A - 0x8000])
	{
		//UDBG("RAINBOW    read from flash %04x => %02x\n", A, flash[A - 0x8000]);
		return flash_prg[A - 0x8000];
	}
	//UDBG("RAINBOW    from PRG\n");
	return CartBR(A);
}

static void RAINBOW_NROMWritePPUFlash(uint32 A, uint8 V)
{
	assert(A <= 0x1fff);
	UDBG("RAINBOW write PPU flash %04x => %02x\n", A, V);
	bool reset_flash_mode = false;
	switch (flash_chr_write_mode)
	{
	case flash_write_mode_t::WRITE_DISABLED:
		flash_chr_write_mode = flash_chr_sequence_counter.addWrite(A, V);
		break;
	case flash_write_mode_t::ERASE_SECTOR:
		UDBG("RAINBOW erase CHR sector %04x %02x\n", A, V);
		if (A == 0x0000 || A == 0x0800 || A == 0x1000 || A == 0x1800)
		{
			::memset(flash_chr + A, 0xff, 0x0800);
			for (uint32 i = A; i < A + 0x0800; ++i)
			{
				flash_chr_written[i] = true;
			}
			for (uint32_t c = 0; c < 0x0800; ++c)
			{
				UDBG("%02x", flash_chr[A + c]);
			}
			UDBG("\n");
		}
		reset_flash_mode = true;
		break;
	case flash_write_mode_t::WRITE_BYTE:
		UDBG("RAINBOW write CHR byte %04x %02x (previous=%02x)\n", A, V, flash_chr[A]);
		flash_chr[A] &= V;
		flash_chr_written[A] = true;
		reset_flash_mode = true;
		break;
	};

	if (reset_flash_mode)
	{
		flash_chr_sequence_counter.reset();
		flash_chr_write_mode = flash_write_mode_t::WRITE_DISABLED;
	}
}

static void RAINBOW_NROMPPUWrite(uint32 A, uint8 V)
{
	if (A < 0x2000)
	{
		RAINBOW_NROMWritePPUFlash(A, V);
	}
	FFCEUX_PPUWrite_Default(A, V);
}

uint8 FASTCALL RAINBOW_NROMPPURead(uint32 A)
{
	if (A < 0x2000 && flash_chr_written[A])
	{
		if (PPU_hook)
			PPU_hook(A);
		return flash_chr[A];
	}
	return FFCEUX_PPURead_Default(A);
}

/* Mapper initialization and cleaning */

static void LatchClose(void)
{
	UDBG("RAINBOW latch close\n");
	if (WRAM)
	{
		FCEU_gfree(WRAM);
	}
	WRAM = NULL;

	if (flash_prg)
	{
		FCEU_gfree(flash_prg);
		FCEU_gfree(flash_prg_written);
	}
	flash_prg = NULL;
	flash_prg_written = NULL;

	if (flash_chr)
	{
		FCEU_gfree(flash_chr);
		FCEU_gfree(flash_chr_written);
	}
	flash_chr = NULL;
	flash_chr_written = NULL;

	delete esp;
}

static void RAINBOW_NROMPower(void)
{
	UDBG("RAINBOW power\n");
	setprg8r(0x10, 0x6000, 0); // Famili BASIC (v3.0) need it (uses only 4KB), FP-BASIC uses 8KB
	setprg16(0x8000, ~1);
	setprg16(0xC000, ~0);
	setchr8(0);

	SetReadHandler(0x6000, 0x7FFF, CartBR);
	SetWriteHandler(0x6000, 0x7FFF, CartBW);

	FCEU_CheatAddRAM(WRAMSIZE >> 10, 0x6000, WRAM);

	SetWriteHandler(0x5000, 0x5000, RAINBOW_NROMWrite);
	SetReadHandler(0x5000, 0x5000, RAINBOW_NROMRead);
	SetWriteHandler(0x5001, 0x5001, RAINBOW_NROMWriteFlags);
	SetReadHandler(0x5001, 0x5001, RAINBOW_NROMReadFlags);

	SetReadHandler(0x8000, 0xFFFF, RAINBOW_NROMReadFlash);
	SetWriteHandler(0x8000, 0xFFFF, RAINBOW_NROMWriteFlash);

	esp = new BrokeStudioFirmware;
	esp_enable = true;
	irq_enable = true;
	flash_prg_sequence_counter.reset();
	flash_prg_write_mode = flash_write_mode_t::WRITE_DISABLED;
	flash_chr_sequence_counter.reset();
	flash_chr_write_mode = flash_write_mode_t::WRITE_DISABLED;
}

void RAINBOW_NROM_Init(CartInfo *info)
{
	UDBG("RAINBOW init\n");
	info->Power = RAINBOW_NROMPower;
	info->Close = LatchClose;

	// Initialize flash PRG
	flash_prg = (uint8 *)FCEU_gmalloc(flash_prg_size);
	flash_prg_written = (bool *)FCEU_gmalloc(flash_prg_size * sizeof(bool));
	::memset(flash_prg_written, 0, flash_prg_size * sizeof(bool));
	//TODO AddExState
	//TODO info->SaveGame[] = flash ; info->SaveGame[] = flash_written

	// Initialize flash CHR
	flash_chr = (uint8 *)FCEU_gmalloc(flash_chr_size);
	flash_chr_written = (bool *)FCEU_gmalloc(flash_chr_size * sizeof(bool));
	::memset(flash_chr_written, 0, flash_chr_size * sizeof(bool));
	//TODO AddExState
	//TODO info->SaveGame[] = flash ; info->SaveGame[] = flash_written

	//TODO is wram really necessary?
	WRAMSIZE = 8192;
	WRAM = (uint8 *)FCEU_gmalloc(WRAMSIZE);
	SetupCartPRGMapping(0x10, WRAM, WRAMSIZE, 1);
	if (info->battery)
	{
		info->SaveGame[0] = WRAM;
		info->SaveGameLen[0] = WRAMSIZE;
	}
	AddExState(WRAM, WRAMSIZE, 0, "WRAM");

	// Set a hook on hblank to be able periodically check if we have to send an interupt
	MapIRQHook = RAINBOW_NROMMapIrq;

	FFCEUX_PPURead = RAINBOW_NROMPPURead;
	FFCEUX_PPUWrite = RAINBOW_NROMPPUWrite;
}
