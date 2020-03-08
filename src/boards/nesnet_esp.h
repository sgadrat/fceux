#ifndef _NESNET_ESP_H_
#define _NESNET_ESP_H_

#include "../types.h"
#include "esp.h"

#include <vector>

class InlFirmware : public EspFirmware {
public:
	~InlFirmware() = default;

	void rx(uint8 v) override;
	uint8 tx() override;

	void setGpio15(bool /*v*/) override {}
	bool getGpio15() override { return false; }

private:
	void cmdHandlerVariable();
	void cmdHandlerSpecial();
	void cmdHandlerMessage();

	uint8 data_register = 0;
	std::vector<uint8> command_buffer;
};

#endif
