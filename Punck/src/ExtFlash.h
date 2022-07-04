#include "initialisation.h"



class ExtFlash {
public:
	enum qspiRegister : uint8_t {pageProgram = 0x02, readData = 0x03, writeEnable = 0x06, readStatusReg1 = 0x05, readStatusReg2 = 0x35, readStatusReg3 = 0x15, writeStatusReg2 = 0x31};

	void Init();
	uint8_t ReadStatus(qspiRegister r);
	void WriteData(uint32_t address, uint8_t data);
	uint8_t ReadData(uint32_t address);

private:


};

extern ExtFlash extFlash;
