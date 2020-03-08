#ifndef _NESNET_ESP_H_
#define _NESNET_ESP_H_

#include "../types.h"
#include "esp.h"

class InlFirmware : public EspFirmware {
public:
	~InlFirmware() = default;

	void rx(uint8 v) override;
	uint8 tx() override;

	void setGpio15(bool /*v*/) override {}
	bool getGpio15() override { return false; }

private:
	void cmdHandlerVariable(uint8 cmd);
	void cmdHandlerSpecial(uint8 cmd);
	void cmdHandlerMessage(uint8 cmd);

	uint8 data_register = 0;
};

#endif
