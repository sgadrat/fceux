#include "mapinc.h"
#include "../ines.h"

#include "easywsclient.hpp"

#include <algorithm>
#include <array>
#include <deque>
#include <limits>
#include <sstream>

using easywsclient::WebSocket;

#undef RAINBOW_DEBUG
#define RAINBOW_DEBUG

#ifdef RAINBOW_DEBUG
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
// BrokeStudio's ESP firmware implementation

class BrokeStudioFirmware: public EspFirmware {
public:
	BrokeStudioFirmware();
	~BrokeStudioFirmware();

	void rx(uint8 v) override;
	uint8 tx() override;

	virtual void setGpio15(bool v) override;
	virtual bool getGpio15() override;

private:
	// Defined message types from CPU to ESP
	enum class n2e_cmds_t : uint8 {
		GET_ESP_STATUS,
		DEBUG_LOG,
		CLEAR_BUFFERS,
		FILES,
		GET_WIFI_STATUS,
		GET_SERVER_STATUS,
		CONNECT_TO_SERVER,
		DISCONNECT_FROM_SERVER,
		SEND_MESSAGE_TO_SERVER,
	};

	// Defined message types from ESP to CPU
	enum class e2n_cmds_t : uint8 {
		READY,
		FILE_LIST,
		FILE_DATA,
		WIFI_STATUS,
		SERVER_STATUS,
		MESSAGE_FROM_SERVER,
	};

	enum class files_subcmd_t : uint8 {
		SELECT,
		GET_LIST,
		DELETE,
		READ,
		READ_AUTO,
		WRITE,
		APPEND,
	};

	void processBufferedMessage();
	void processFileMessage();
	void readFile(uint8 file, uint8 n, uint32 offset);
	template<class I>
	void writeFile(uint8 file, uint32 offset, I data_begin, I data_end);

	template<class I>
	void sendMessageToServer(I begin, I end);
	void receiveDataFromServer();

private:
	std::deque<uint8> rx_buffer;
	std::deque<uint8> tx_buffer;

	std::array<std::vector<uint8>, 64> files;
	std::array<bool, 64> file_exists;
	uint32 file_offset = 0;
	uint8 working_file = 0;

	WebSocket::pointer socket = nullptr;

	bool msg_first_byte = true;
	uint8 msg_length = 0;
	uint8 last_byte_read = 0;
};

BrokeStudioFirmware::BrokeStudioFirmware() {
	UDBG("RAINBOW BrokeStudioFirmware ctor\n");
	WebSocket::pointer ws = WebSocket::from_url("ws://localhost:3000");
	if (!ws) {
		UDBG("RAINBOW unable to connect to server");
		return;
	}
	this->socket = ws;
}

BrokeStudioFirmware::~BrokeStudioFirmware() {
	UDBG("RAINBOW BrokeStudioFirmware dtor\n");
	if (this->socket != nullptr) {
		delete this->socket;
		this->socket = nullptr;
	}
}

void BrokeStudioFirmware::rx(uint8 v) {
	UDBG("RAINBOW BrokeStudioFirmware rx %02x\n", v);
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

uint8 BrokeStudioFirmware::tx() {
	UDBG("RAINBOW BrokeStudioFirmware tx\n");

	// Refresh buffer from network
	this->receiveDataFromServer();

	// Get byte from buffer
	if (!this->tx_buffer.empty()) {
		last_byte_read = this->tx_buffer.front();
		this->tx_buffer.pop_front();
	}

	UDBG("RAINBOW BrokeStudioFirmware tx <= %02x\n", last_byte_read);
	return last_byte_read;
}

void BrokeStudioFirmware::setGpio15(bool /*v*/) {
}

bool BrokeStudioFirmware::getGpio15() {
	this->receiveDataFromServer();
	return !this->tx_buffer.empty();
}

void BrokeStudioFirmware::processBufferedMessage() {
	if (this->rx_buffer.size() >= 2) {
		// Process the message in RX buffer
		switch (static_cast<n2e_cmds_t>(this->rx_buffer.at(1))) {
			case n2e_cmds_t::GET_ESP_STATUS:
				UDBG("RAIBOW BrokeStudioFirmware received message GET_ESP_STATUS");
				this->tx_buffer.push_back(last_byte_read);
				this->tx_buffer.push_back(1);
				this->tx_buffer.push_back(static_cast<uint8>(e2n_cmds_t::READY));
				break;
			case n2e_cmds_t::DEBUG_LOG:
				#ifdef RAINBOW_DEBUG
					FCEU_printf("RAINBOW DEBUG/LOG: ");
					for (std::deque<uint8>::const_iterator cur = this->rx_buffer.begin() + 2; cur < this->rx_buffer.end(); ++cur) {
						FCEU_printf("%02x ", *cur);
					}
					FCEU_printf("\n");
				#endif
				break;
			case n2e_cmds_t::CLEAR_BUFFERS:
				UDBG("RAINBOW BrokeStudioFirmware received message CLEAR_BUFFERS\n");
				this->tx_buffer.clear();
				this->rx_buffer.clear();
				break;
			case n2e_cmds_t::FILES:
				this->processFileMessage();
				break;
			case n2e_cmds_t::GET_WIFI_STATUS:
				UDBG("RAINBOW BrokeStudioFirmware received message GET_WIFI_STATUS\n");
				this->tx_buffer.push_back(last_byte_read);
				this->tx_buffer.push_back(2);
				this->tx_buffer.push_back(static_cast<uint8>(e2n_cmds_t::WIFI_STATUS));
				this->tx_buffer.push_back(3); // Simple answer, wifi is ok
				break;
			case n2e_cmds_t::GET_SERVER_STATUS:
				UDBG("RAINBOW BrokeStudioFirmware received message GET_SERVER_STATUS\n");
				this->tx_buffer.push_back(last_byte_read);
				this->tx_buffer.push_back(2);
				this->tx_buffer.push_back(static_cast<uint8>(e2n_cmds_t::SERVER_STATUS));
				this->tx_buffer.push_back(this->socket != nullptr); // Server connection is ok if we succeed to open it
				break;
			case n2e_cmds_t::CONNECT_TO_SERVER:
				// TODO
				break;
			case n2e_cmds_t::DISCONNECT_FROM_SERVER:
				// TODO
				break;
			case n2e_cmds_t::SEND_MESSAGE_TO_SERVER: {
				UDBG("RAINBOW BrokeStudioFirmware received message SEND_MESSAGE\n");
				uint8 const payload_size = this->rx_buffer.size() - 2;
				std::deque<uint8>::const_iterator payload_begin = this->rx_buffer.begin() + 2;
				std::deque<uint8>::const_iterator payload_end = payload_begin + payload_size;
				this->sendMessageToServer(payload_begin, payload_end);
				break;
			}
			default:
				UDBG("RAINBOW BrokeStudioFirmware received unknown message %02x\n", this->rx_buffer.at(1));
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

void BrokeStudioFirmware::processFileMessage() {
	assert(this->rx_buffer.size() >= 2);
	uint8 message_size = this->rx_buffer.front();
	if (message_size >= 2 && this->rx_buffer.size() >= static_cast<std::deque<uint8>::size_type>(message_size) + 1) {
		switch (static_cast<files_subcmd_t>(this->rx_buffer.at(2))) {
			case files_subcmd_t::SELECT:
				if (message_size == 3) {
					uint8 const selected_file = this->rx_buffer.at(3);
					if (selected_file < this->files.size()) {
						this->working_file = selected_file;
						this->file_offset = 0;
					}
				}
				break;
			case files_subcmd_t::GET_LIST:
				{
					UDBG("RAINBOW received FILES.GET_LIST\n");
					std::vector<uint8> existing_files;
					assert(this->file_exists.size() < 254);
					for (uint8 i = 0; i < this->file_exists.size(); ++i) {
						if (this->file_exists[i]) {
							existing_files.push_back(i);
						}
					}
					this->tx_buffer.push_back(last_byte_read);
					this->tx_buffer.push_back(existing_files.size() + 2);
					this->tx_buffer.push_back(static_cast<uint8>(e2n_cmds_t::FILE_LIST));
					this->tx_buffer.push_back(existing_files.size());
					for (uint8 i: existing_files) {
						UDBG("RAINBOW => %02x\n", i);
						this->tx_buffer.push_back(i);
					}
				}
				break;
			case files_subcmd_t::DELETE:
				if (message_size == 3) {
					uint8 file_index = this->rx_buffer[3];
					if (file_index < this->files.size()) {
						assert(file_index < this->file_exists.size());
						this->files[file_index].clear();
						this->file_exists[file_index] = false;
					}
				}
				break;
			case files_subcmd_t::READ:
				if (message_size >= 4) {
					uint8 n = this->rx_buffer[3];
					uint32 offset = 0;
					for (uint8 i = 4; i <= message_size; ++i) {
						offset = (offset << 8) + i;
					}
					this->readFile(this->working_file, n, offset);
				}
				break;
			case files_subcmd_t::READ_AUTO:
				if (message_size >= 3) {
					uint8 const n = this->rx_buffer[3];
					this->readFile(this->working_file, n, this->file_offset);
					this->file_offset += n;
					if (this->file_offset > this->files[this->working_file].size()) {
						this->file_offset = this->files[this->working_file].size();
					}
				}
				break;
			case files_subcmd_t::WRITE:
				UDBG("RAINBOW write");
				if (message_size >= 8 && this->rx_buffer[7] == message_size - 7) {
					uint32 offset = this->rx_buffer[6];
					offset = (offset << 8) + this->rx_buffer[5];
					offset = (offset << 8) + this->rx_buffer[4];
					offset = (offset << 8) + this->rx_buffer[3];

					this->writeFile(this->working_file, offset, this->rx_buffer.begin() + 8, this->rx_buffer.begin() + message_size);
				}
				break;
			case files_subcmd_t::APPEND:
				if (message_size >= 4 && this->rx_buffer[3] == message_size - 3) {
					this->writeFile(this->working_file, this->file_offset, this->rx_buffer.begin() + 4, this->rx_buffer.begin() + message_size);
					this->file_offset += message_size - 3;
				}
				break;
			default:
				break;
		};
	}
}

void BrokeStudioFirmware::readFile(uint8 file, uint8 n, uint32 offset) {
	assert(file < this->files.size());

	// Get data range
	std::vector<uint8> const& f = this->files[file];
	std::vector<uint8>::const_iterator data_begin;
	std::vector<uint8>::const_iterator data_end;
	if (offset >= f.size()) {
		data_begin = f.end();
		data_end = data_begin;
	}else {
		data_begin = f.begin() + offset;
		data_end = f.begin() + std::min(static_cast<std::vector<uint8>::size_type>(offset) + n, f.size());
	}
	std::vector<uint8>::size_type const data_size = data_end - data_begin;

	// Write response
	this->tx_buffer.push_back(last_byte_read);
	this->tx_buffer.push_back(data_size + 2);
	this->tx_buffer.push_back(static_cast<uint8>(e2n_cmds_t::FILE_DATA));
	this->tx_buffer.push_back(data_size);
	while (data_begin != data_end) {
		this->tx_buffer.push_back(*data_begin);
		++data_begin;
	}
}

template<class I>
void BrokeStudioFirmware::writeFile(uint8 file, uint32 offset, I data_begin, I data_end) {
	std::vector<uint8>& f = this->files[file];
	auto const data_size = data_end - data_begin;
	uint32 const offset_end = offset + data_size;
	if (offset_end > f.size()) {
		f.resize(offset_end, 0);
	}

	for (std::vector<uint8>::size_type i = offset; i < offset_end; ++i) {
		f[i] = *data_begin;
		++data_begin;
	}
	this->file_exists[file] = true;
}

template<class I>
void BrokeStudioFirmware::sendMessageToServer(I begin, I end) {
#ifdef RAINBOW_DEBUG
	FCEU_printf("RAINBOW message to send: ");
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

void BrokeStudioFirmware::receiveDataFromServer() {
	if (this->socket == nullptr) {
		return;
	}

	this->socket->poll();
	this->socket->dispatchBinary([this] (std::vector<uint8_t> const& data) {
		size_t const msg_len = data.end() - data.begin();
		if (msg_len <= 0xff) {
			UDBG("RAINBOW WebSocket data received... size %02x, msg: ", msg_len);
			for (uint8_t const c: data) {
				UDBG("%02x", c);
			}
			UDBG("\n");
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

/* Flash write unlock sequence logic */

enum class flash_write_mode_t : uint8 {
	WRITE_DISABLED,
	ERASE_SECTOR,
	WRITE_BYTE,
};

class FlashSequenceCounter {
public:
	FlashSequenceCounter(std::array<uint32, 5> const& sequence_addresses);
	void reset();
	flash_write_mode_t addWrite(uint32 address, uint8 value);
private:
	std::array<uint32, 5> sequence_addresses;
	uint8 sequence_position = 0;
};

FlashSequenceCounter::FlashSequenceCounter(std::array<uint32, 5> const& sequence_addresses)
: sequence_addresses(sequence_addresses)
{
}

void FlashSequenceCounter::reset() {
	this->sequence_position = 0;
}

flash_write_mode_t FlashSequenceCounter::addWrite(uint32 address, uint8 value) {
	uint8 const sequence_values[5] = {0xaa, 0x55, 0x80, 0xaa, 0x55};

	if (address == this->sequence_addresses[this->sequence_position] && value == sequence_values[this->sequence_position]) {
		++this->sequence_position;
		if (this->sequence_position == 5) {
			return flash_write_mode_t::ERASE_SECTOR;
		}else {
			return flash_write_mode_t::WRITE_DISABLED;
		}
	}else if (this->sequence_position == 2 && value == 0xa0) {
		return flash_write_mode_t::WRITE_BYTE;
	}else {
		this->reset();
		return flash_write_mode_t::WRITE_DISABLED;
	}
}

/* Mapper state */

static uint8 *WRAM = NULL;
static uint32 WRAMSIZE;
static uint8 * flash_prg = NULL;
static bool * flash_prg_written = NULL;
static uint32 const flash_prg_size = 32 * 1024;
static uint8 * flash_chr = NULL;
static bool * flash_chr_written = NULL;
static uint32 const flash_chr_size = 8 * 1024;
static EspFirmware *esp = NULL;
static bool esp_enable = true;
static bool irq_enable = true;
static FlashSequenceCounter flash_prg_sequence_counter({0xd555, 0xaaaa, 0xd555, 0xd555, 0xaaaa});
static flash_write_mode_t flash_prg_write_mode = flash_write_mode_t::WRITE_DISABLED;
static FlashSequenceCounter flash_chr_sequence_counter({0x1555, 0x0aaa, 0x1555, 0x1555, 0x0aaa});
static flash_write_mode_t flash_chr_write_mode = flash_write_mode_t::WRITE_DISABLED;

/* ESP interface */

static DECLFW(RAINBOWWrite) {
	UDBG("RAINBOW write %04x %02x\n", A, V);
	esp->rx(V);
}

static DECLFR(RAINBOWRead) {
	UDBG("RAINBOW read %04x\n", A);
	return esp->tx();
}

static DECLFW(RAINBOWWriteFlags) {
	UDBG("RAINBOW write %04x %02x\n", A, V);
	esp_enable = V & 0x01;
	irq_enable = V & 0x40;
}

static DECLFR(RAINBOWReadFlags) {
	uint8 esp_rts_flag = esp->getGpio15() ? 0x80 : 0x00;
	uint8 esp_enable_flag = esp_enable ? 0x01 : 0x00;
	uint8 irq_enable_flag = irq_enable ? 0x40 : 0x00;
	UDBG("RAINBOW read flags %04x => %02x\n", A, esp_rts_flag | esp_enable_flag | irq_enable_flag);
	return esp_rts_flag | esp_enable_flag | irq_enable_flag;
}

static void RAINBOWMapIrq(int32) {
	if (irq_enable) {
		if (esp->getGpio15()) {
			X6502_IRQBegin(FCEU_IQEXT);
		} else {
			X6502_IRQEnd(FCEU_IQEXT);
		}
	}
}

/* Flash memory writing logic */

DECLFW(RAINBOWWriteFlash) {
	assert(0x8000 <= A && A <= 0xffff);
	UDBG("RAINBOW write flash %04x => %02x\n", A, V);
	bool reset_flash_mode = false;
	switch (flash_prg_write_mode) {
		case flash_write_mode_t::WRITE_DISABLED:
			flash_prg_write_mode = flash_prg_sequence_counter.addWrite(A, V);
			break;
		case flash_write_mode_t::ERASE_SECTOR:
			UDBG("RAINBOW erase sector %04x %02x\n", A, V);
			if (A == 0x8000 || A == 0x9000 || A == 0xa000 || A == 0xb000 || A == 0xc000 || A == 0xd000 || A == 0xe000 || A == 0xf000) {
				::memset(flash_prg + (A - 0x8000), 0xff, 0x1000);
				for (uint32 i = A; i < A + 0x1000; ++i) {
					flash_prg_written[i - 0x8000] = true;
				}
				for (uint32_t c = 0; c < 0x1000; ++c) {
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

	if (reset_flash_mode) {
		flash_prg_sequence_counter.reset();
		flash_prg_write_mode = flash_write_mode_t::WRITE_DISABLED;
	}
}

DECLFR(RAINBOWReadFlash) {
	assert(0x8000 <= A && A <= 0xffff);
	//UDBG("RAINBOW read flash %04x\n", A);
	if (flash_prg_written[A - 0x8000]) {
		//UDBG("RAINBOW    read from flash %04x => %02x\n", A, flash[A - 0x8000]);
		return flash_prg[A - 0x8000];
	}
	//UDBG("RAINBOW    from PRG\n");
	return CartBR(A);
}

static void RAINBOWWritePPUFlash(uint32 A, uint8 V) {
	assert(A <= 0x1fff);
	UDBG("RAINBOW write PPU flash %04x => %02x\n", A, V);
	bool reset_flash_mode = false;
	switch (flash_chr_write_mode) {
		case flash_write_mode_t::WRITE_DISABLED:
			flash_chr_write_mode = flash_chr_sequence_counter.addWrite(A, V);
			break;
		case flash_write_mode_t::ERASE_SECTOR:
			UDBG("RAINBOW erase CHR sector %04x %02x\n", A, V);
			if (A == 0x0000 || A == 0x0800 || A == 0x1000 || A == 0x1800) {
				::memset(flash_chr + A, 0xff, 0x0800);
				for (uint32 i = A; i < A + 0x0800; ++i) {
					flash_chr_written[i] = true;
				}
				for (uint32_t c = 0; c < 0x0800; ++c) {
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

	if (reset_flash_mode) {
		flash_chr_sequence_counter.reset();
		flash_chr_write_mode = flash_write_mode_t::WRITE_DISABLED;
	}
}

static void RAINBOWPPUWrite(uint32 A, uint8 V) {
	if (A < 0x2000) {
		RAINBOWWritePPUFlash(A, V);
	}
	FFCEUX_PPUWrite_Default(A, V);
}

uint8 FASTCALL RAINBOWPPURead(uint32 A) {
	if (A < 0x2000 && flash_chr_written[A]) {
		if (PPU_hook) PPU_hook(A);
		return flash_chr[A];
	}
	return FFCEUX_PPURead_Default(A);
}

/* Mapper initialization and cleaning */

static void LatchClose(void) {
	UDBG("RAINBOW latch close\n");
	if (WRAM) {
		FCEU_gfree(WRAM);
	}
	WRAM = NULL;

	if (flash_prg) {
		FCEU_gfree(flash_prg);
		FCEU_gfree(flash_prg_written);
	}
	flash_prg = NULL;
	flash_prg_written = NULL;

	if (flash_chr) {
		FCEU_gfree(flash_chr);
		FCEU_gfree(flash_chr_written);
	}
	flash_chr = NULL;
	flash_chr_written = NULL;

	delete esp;
}

static void RAINBOWPower(void) {
	UDBG("RAINBOW power\n");
	setprg8r(0x10, 0x6000, 0);	// Famili BASIC (v3.0) need it (uses only 4KB), FP-BASIC uses 8KB
	setprg16(0x8000, ~1);
	setprg16(0xC000, ~0);
	setchr8(0);

	SetReadHandler(0x6000, 0x7FFF, CartBR);
	SetWriteHandler(0x6000, 0x7FFF, CartBW);

	FCEU_CheatAddRAM(WRAMSIZE >> 10, 0x6000, WRAM);

	SetWriteHandler(0x5000, 0x5000, RAINBOWWrite);
	SetReadHandler(0x5000, 0x5000, RAINBOWRead);
	SetWriteHandler(0x5001, 0x5001, RAINBOWWriteFlags);
	SetReadHandler(0x5001, 0x5001, RAINBOWReadFlags);

	SetReadHandler(0x8000, 0xFFFF, RAINBOWReadFlash);
	SetWriteHandler(0x8000, 0xFFFF, RAINBOWWriteFlash);

	esp = new BrokeStudioFirmware;
	esp_enable = true;
	irq_enable = true;
	flash_prg_sequence_counter.reset();
	flash_prg_write_mode = flash_write_mode_t::WRITE_DISABLED;
	flash_chr_sequence_counter.reset();
	flash_chr_write_mode = flash_write_mode_t::WRITE_DISABLED;
}

void RAINBOW_Init(CartInfo *info) {
	UDBG("RAINBOW init\n");
	info->Power = RAINBOWPower;
	info->Close = LatchClose;

	// Initialize flash PRG
	flash_prg = (uint8*)FCEU_gmalloc(flash_prg_size);
	flash_prg_written = (bool*)FCEU_gmalloc(flash_prg_size * sizeof(bool));
	::memset(flash_prg_written, 0, flash_prg_size * sizeof(bool));
	//TODO AddExState
	//TODO info->SaveGame[] = flash ; info->SaveGame[] = flash_written

	// Initialize flash CHR
	flash_chr = (uint8*)FCEU_gmalloc(flash_chr_size);
	flash_chr_written = (bool*)FCEU_gmalloc(flash_chr_size * sizeof(bool));
	::memset(flash_chr_written, 0, flash_chr_size * sizeof(bool));
	//TODO AddExState
	//TODO info->SaveGame[] = flash ; info->SaveGame[] = flash_written

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
	MapIRQHook = RAINBOWMapIrq;

	FFCEUX_PPURead = RAINBOWPPURead;
	FFCEUX_PPUWrite = RAINBOWPPUWrite;
}
