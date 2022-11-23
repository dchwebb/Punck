#pragma once

#include "initialisation.h"
#include "USBHandler.h"

// MSC defines
#define BOT_GET_MAX_LUN						0xFE

#define USBD_BOT_CBW_SIGNATURE             0x43425355U
#define USBD_BOT_CSW_SIGNATURE             0x53425355U

// SCSI Commands
#define SCSI_FORMAT_UNIT                            0x04U
#define SCSI_INQUIRY                                0x12U
#define SCSI_MODE_SELECT6                           0x15U
#define SCSI_MODE_SELECT10                          0x55U
#define SCSI_MODE_SENSE6                            0x1AU
#define SCSI_MODE_SENSE10                           0x5AU
#define SCSI_ALLOW_MEDIUM_REMOVAL                   0x1EU
#define SCSI_READ6                                  0x08U
#define SCSI_READ10                                 0x28U
#define SCSI_READ12                                 0xA8U
#define SCSI_READ16                                 0x88U

#define SCSI_READ_CAPACITY10                        0x25U
#define SCSI_READ_CAPACITY16                        0x9EU

#define SCSI_REQUEST_SENSE                          0x03U
#define SCSI_START_STOP_UNIT                        0x1BU
#define SCSI_TEST_UNIT_READY                        0x00U
#define SCSI_WRITE6                                 0x0AU
#define SCSI_WRITE10                                0x2AU
#define SCSI_WRITE12                                0xAAU
#define SCSI_WRITE16                                0x8AU

#define SCSI_VERIFY10                               0x2FU
#define SCSI_VERIFY12                               0xAFU
#define SCSI_VERIFY16                               0x8FU

#define SCSI_SEND_DIAGNOSTIC                        0x1DU
#define SCSI_READ_FORMAT_CAPACITIES                 0x23U

//SCSI Sense error codes
#define NO_SENSE                                    0U
#define RECOVERED_ERROR                             1U
#define NOT_READY                                   2U
#define MEDIUM_ERROR                                3U
#define HARDWARE_ERROR                              4U
#define ILLEGAL_REQUEST                             5U
#define UNIT_ATTENTION                              6U
#define DATA_PROTECT                                7U
#define BLANK_CHECK                                 8U
#define VENDOR_SPECIFIC                             9U
#define COPY_ABORTED                                10U
#define ABORTED_COMMAND                             11U
#define VOLUME_OVERFLOW                             13U
#define MISCOMPARE                                  14U

#define INVALID_CDB                                 0x20U
#define INVALID_FIELD_IN_COMMAND                    0x24U
#define PARAMETER_LIST_LENGTH_ERROR                 0x1AU
#define INVALID_FIELD_IN_PARAMETER_LIST             0x26U
#define ADDRESS_OUT_OF_RANGE                        0x21U
#define MEDIUM_NOT_PRESENT                          0x3AU
#define MEDIUM_HAVE_CHANGED                         0x28U
#define WRITE_PROTECTED                             0x27U
#define UNRECOVERED_READ_ERROR                      0x11U
#define WRITE_FAULT                                 0x03U

#define REQUEST_SENSE_DATA_LEN                      0x12U

// SCSI Meduim codes
#define SCSI_MEDIUM_UNLOCKED                        0x00U
#define SCSI_MEDIUM_LOCKED                          0x01U
#define SCSI_MEDIUM_EJECTED                         0x02U





class USB;

class MSCHandler : public USBHandler {
public:
	MSCHandler(USB* usb, uint8_t inEP, uint8_t outEP, int8_t interface) : USBHandler(usb, inEP, outEP, interface) {
		outBuff = xfer_buff;
	}

	void DataIn() override;
	void DataOut() override;
	void ClassSetup(usbRequest& req) override;
	void ClassSetupData(usbRequest& req, const uint8_t* data) override;
	uint32_t GetInterfaceDescriptor(const uint8_t** buffer) override;

	void DMATransferDone();

	static const uint8_t Descriptor[];

private:
	enum class BotState {Idle, DataOut, DataIn, LastDataIn, SendData, NoData};
	enum CSWStatus {CSWCmdPassed = 0, CSWCmdFailed = 1, CSWCmdPhaseError = 2};
	inline constexpr static uint32_t MediaPacket = 512;

	void MSC_BOT_CBW_Decode();
	void MSC_BOT_SendCSW(uint8_t CSW_Status);
	void MSC_BOT_Abort();

	int8_t SCSI_ProcessCmd();
	int8_t SCSI_Inquiry();
	int8_t SCSI_ReadFormatCapacity();
	int8_t SCSI_ReadCapacity10();
	int8_t SCSI_ReadCapacity16();
	int8_t SCSI_ModeSense10();
	int8_t SCSI_ModeSense6();
	int8_t SCSI_Read();
	void ReadReady();
	int8_t SCSI_Write();
	int8_t SCSI_CheckAddressRange(uint32_t blk_offset, uint32_t blk_nbr);
	int8_t SCSI_TestUnitReady();
	int8_t SCSI_AllowPreventRemovable();
	int8_t SCSI_Verify10();
	void SCSI_SenseCode(uint8_t sKey, uint8_t ASC);
	int8_t SCSI_RequestSense();

	uint32_t xfer_buff[512];				// EP1 (MSC) OUT Data filled in RxLevel Interrupt

	const uint8_t maxLUN = 0;				// MSC - maximum index of logical devices
	BotState bot_state = BotState::Idle;
	uint32_t bot_data_length = 0;
	const uint8_t* botBuff;
	uint8_t bot_data[MediaPacket];
	uint32_t scsi_blk_addr;
	uint32_t scsi_blk_len;
	uint32_t scsi_medium_state = 0;



	// Command Status Wrapper
	inline constexpr static uint8_t cswSize = 13;	// Can't use sizeof as includes padding
	struct {
		uint32_t dSignature;				// Always 53 42 53 55
		uint32_t dTag;
		uint32_t dDataResidue;
		uint8_t  bStatus;
		uint8_t  ReservedForAlign[3];
	} csw;

	// Command Block Wrapper
	inline constexpr static uint8_t cbwSize = 31;
	struct {
		uint32_t dSignature;				// Always 43 42 53 55
		uint32_t dTag;
		uint32_t dDataLength;
		uint8_t  bmFlags;
		uint8_t  bLUN;
		uint8_t  bCBLength;
		uint8_t  CB[16];
		uint8_t  ReservedForAlign;
	} cbw;

	inline static constexpr uint8_t SenseListDepth = 4;
	uint8_t scsi_sense_head;
	uint8_t scsi_sense_tail;

	struct SCSISenseItem {
		uint8_t Skey;
		uint8_t ASC;
		uint8_t ASCQ;
	};
	SCSISenseItem scsi_sense[SenseListDepth];		// Ring buffer to hold error codes

	// USB Mass storage Page 0 Inquiry Data (Supported VPD pages SPC-3 345)
	constexpr static uint8_t MSC_Page00_Inquiry_Data[] = {
			0x00,
			0x00,
			0x00,
			0x02,							// Number of pages
			0x00,							// Supports Page 0x00 (Supported VPD Pages)
			0x80							// Supports Page 0x80 (Unit Serial Number)
	};

	// USB Mass storage VPD Page 0x80 Inquiry Data for Unit Serial Number
	constexpr static uint8_t MSC_Page80_Inquiry_Data[] = {
			0x00,
			0x80,
			0x00,
			0x04,							// Page Length
			0x20,							// Product Serial number
			0x20,
			0x20,
			0x20
	};

	// USB Mass storage Standard Inquiry Data (See p144 of SPC-3)
	constexpr static uint8_t STORAGE_Inquirydata_FS[] = {
			0x00,							// Peripheral qualifier and device type (0 = Direct access block device)
			0x80,							// RMB (Removable media bit) 1= media removable
			0x02,							// Version (2 = obsolete??)
			0x02,							// RESPONSE DATA FORMAT
			0x1F,							// Size of data below
			0x00,							// SCCS | ACC | TPGS | 3PC | Reserved | PROTECT
			0x00,							// BQUE | ENCSERV | VS | MULTIP | MCHNGR | Obsolete | Obsolete | ADDR16
			0x00,							// Obsolete | Obsolete | WBUS16 | SYNC | LINKED | Obsolete | CMDQUE | VS
			'M', 'o', 'u', 'n', 't', 'j', 'o', 'y',		// Manufacturer : 8 bytes
			'P', 'u', 'n', 'c', 'k', ' ', ' ', ' ',		// Product      : 16 Bytes
			' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
			'0', '.', '0' ,'1'							// Version      : 4 Bytes
	};

	// FIXME - as these arrays are mostly blank it will probably be easier to initialise to zero and set non-zero values
	// USB Mass storage sense 10 Data (See SPC-3 p.279)
	static constexpr uint8_t MSC_Mode_Sense10_data[] = {
			0x00,
			0x26,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x08,
			0x12,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00
	};

	// USB Mass storage sense 6 Data (See SPC-3 p.279)
	static constexpr uint8_t MSC_Mode_Sense6_data[] = {
			0x22,
			0x00,
			0x00,
			0x00,
			0x08,
			0x12,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00,
			0x00
	};
};
