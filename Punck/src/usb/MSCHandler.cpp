
#include "USB.h"
#include "MSCHandler.h"
#include "ExtFlash.h"
#include "FatTools.h"

void MSCHandler::DataIn()
{
	switch (bot_state) {
	case BotState::DataIn:
		if (SCSI_ProcessCmd() < 0) {
			MSC_BOT_SendCSW(CSWCmdFailed);
		}
		break;

	case BotState::SendData:
	case BotState::LastDataIn:
		MSC_BOT_SendCSW(CSWCmdPassed);
		break;

	default:
		break;
	}
}


void MSCHandler::DataOut()
{
	if (outBuff[0] == USBD_BOT_CBW_SIGNATURE) {
		memcpy(&cbw, outBuff, sizeof(cbw));
	}

	switch (bot_state) {
	case BotState::Idle:
		MSC_BOT_CBW_Decode();
		break;

	case BotState::DataOut:
		if (SCSI_ProcessCmd() < 0) {
			MSC_BOT_SendCSW(CSWCmdFailed);
		}
		break;

	default:
		break;
	}
}


void MSCHandler::ClassSetup(usbRequest& req)
{
	switch (req.Request) {
	case BOT_GET_MAX_LUN:
		if ((req.Value == 0) && ((req.RequestType & 0x80U) == 0x80U)) {
			SetupIn(req.Length, &maxLUN);
		}
		break;
	}
}


void MSCHandler::ClassSetupData(usbRequest& req, const uint8_t* data)
{

}


void MSCHandler::MSC_BOT_CBW_Decode()
{
	csw.dTag = cbw.dTag;
	csw.dDataResidue = cbw.dDataLength;

	if ((outBuffCount != cbwSize) || (cbw.dSignature != USBD_BOT_CBW_SIGNATURE) || (cbw.bLUN > 1U) || (cbw.bCBLength < 1U) || (cbw.bCBLength > 16U)) {
		SCSI_SenseCode(ILLEGAL_REQUEST, INVALID_CDB);
		MSC_BOT_Abort();
	} else {
		if (SCSI_ProcessCmd() < 0) {

			if (bot_state == BotState::NoData) {
				MSC_BOT_SendCSW(CSWCmdFailed);
			} else {
				MSC_BOT_Abort();
			}

		} else if ((bot_state != BotState::DataIn) && (bot_state != BotState::DataOut) && (bot_state != BotState::LastDataIn))	{
			if (bot_data_length > 0) {

				const uint32_t length = std::min(cbw.dDataLength, bot_data_length);

				csw.dDataResidue -= bot_data_length;
				csw.bStatus = CSWCmdPassed;
				bot_state = BotState::SendData;

				inBuff = botBuff;
				inBuffSize = length;
				inBuffCount = 0;

				EndPointTransfer(Direction::in, inEP, length);

			} else {
				MSC_BOT_SendCSW(CSWCmdPassed);
			}
		}
	}
}


void MSCHandler::MSC_BOT_SendCSW(uint8_t CSW_Status)
{
	csw.dSignature = USBD_BOT_CSW_SIGNATURE;
	csw.bStatus = CSW_Status;
	bot_state = BotState::Idle;

	inBuff = reinterpret_cast<uint8_t*>(&csw);
	inBuffSize = cswSize;
	inBuffCount = 0;

	EndPointTransfer(Direction::in, inEP, inBuffSize);

	outBuffCount = cbwSize;
	EndPointTransfer(Direction::out, outEP, outBuffCount);
}


void MSCHandler::MSC_BOT_Abort()
{
	// Stall the MSC endpoints
	USBx_INEP(inEP)->DIEPCTL |= USB_OTG_DIEPCTL_STALL;
	USBx_OUTEP(outEP)->DOEPCTL |= USB_OTG_DOEPCTL_STALL;
}


int8_t MSCHandler::SCSI_ProcessCmd()
{
#if (USB_DEBUG)
	usb->USBUpdateDbg({}, {}, {}, {}, cbw.CB[0], nullptr);
#endif
	switch (cbw.CB[0])
	{
	case SCSI_TEST_UNIT_READY:					// 0x00
		return SCSI_TestUnitReady();
		break;

	case SCSI_INQUIRY:							// 0x12
		return SCSI_Inquiry();
		break;

	case SCSI_MODE_SENSE6:						// 0x1A
		return SCSI_ModeSense6();
		break;

	case SCSI_ALLOW_MEDIUM_REMOVAL:				// 0x1E
		return SCSI_AllowPreventRemovable();
		break;

	case SCSI_MODE_SENSE10:						// 0x5A
		return SCSI_ModeSense10();
		break;

	case SCSI_READ_FORMAT_CAPACITIES:			// 0x23
		return SCSI_ReadFormatCapacity();
		break;

	case SCSI_READ_CAPACITY10:					// 0x25
		return SCSI_ReadCapacity10();
		break;

	case SCSI_READ10:							// 0x28
		return SCSI_Read();
		break;

	case SCSI_READ12:							// 0xA8 Untested but no obvious difference between Read10 and Read12
		return SCSI_Read();
		break;

	case SCSI_WRITE10:							// 0x2A Same command for Write10 and Write16 as only minor difference
		return SCSI_Write();
		break;

	case SCSI_WRITE12:							// 0xAA Untested
		return SCSI_Write();
		break;

	case SCSI_REQUEST_SENSE:					// 0x03
		return SCSI_RequestSense();
		break;

	case SCSI_READ_CAPACITY16:					// 0x9E
		return SCSI_ReadCapacity16();
		break;

	case SCSI_VERIFY10:							// 0x2F
		return SCSI_Verify10();
		break;

    case SCSI_START_STOP_UNIT:
      return 0;
      break;

	default:
		SCSI_SenseCode(ILLEGAL_REQUEST, INVALID_CDB);
		return -1;
		break;
	}
	return 0;
}


int8_t MSCHandler::SCSI_Inquiry()
{
	if (cbw.dDataLength == 0) {
		SCSI_SenseCode(ILLEGAL_REQUEST, INVALID_CDB);
		return -1;
	}

	if (cbw.CB[1] & 1) {								// Evpd (enable vital product data) is set
		if (cbw.CB[2] == 0)  {							// Request for Supported Vital Product Data Pages (p321 of SPC-3)
			bot_data_length = sizeof(MSC_Page00_Inquiry_Data);
			botBuff = MSC_Page00_Inquiry_Data;

		} else if (cbw.CB[2] == 0x80)  {				// Request for VPD page 0x80 Unit Serial Number
			bot_data_length = sizeof(MSC_Page80_Inquiry_Data);
			botBuff = MSC_Page80_Inquiry_Data;

		} else {										// Request Not supported
			SCSI_SenseCode(ILLEGAL_REQUEST, INVALID_FIELD_IN_COMMAND);
			return -1;
		}
	} else {
		bot_data_length = std::min((uint8_t)sizeof(STORAGE_Inquirydata_FS), cbw.CB[4]);
		botBuff = STORAGE_Inquirydata_FS;
	}

	return 0;
}


int8_t MSCHandler::SCSI_ReadFormatCapacity()
{
	const uint32_t blk_nbr = fatSectorCount - 1;
	const uint32_t blk_size = fatSectorSize;

	*(uint32_t*)&bot_data = 0; 						// blank out first three bytes of bot_data as reserved

	uint8_t* blk8 = (uint8_t*)&blk_nbr;

	bot_data[3] = 0x08;
	bot_data[4] = blk8[3];
	bot_data[5] = blk8[2];
	bot_data[6] = blk8[1];
	bot_data[7] = blk8[0];

	blk8 = (uint8_t*)&blk_size;

	bot_data[8] = 0x02;
	bot_data[9] = blk8[2];
	bot_data[10] = blk8[1];
	bot_data[11] = blk8[0];

	botBuff = bot_data;
	bot_data_length = 12;

	return 0;
}


int8_t MSCHandler::SCSI_ReadCapacity10()
{
	const uint32_t blk_nbr = fatSectorCount - 1;
	const uint32_t blk_size = fatSectorSize;

	uint8_t* blk8 = (uint8_t*)&blk_nbr;

	bot_data[0] = blk8[3];
	bot_data[1] = blk8[2];
	bot_data[2] = blk8[1];
	bot_data[3] = blk8[0];

	blk8 = (uint8_t*)&blk_size;

	bot_data[4] = blk8[3];
	bot_data[5] = blk8[2];
	bot_data[6] = blk8[1];
	bot_data[7] = blk8[0];

	botBuff = bot_data;
	bot_data_length = 8;

	return 0;
}


int8_t MSCHandler::SCSI_ReadCapacity16()		// Untested
{
	const uint32_t blk_nbr = fatSectorCount - 1;
	const uint32_t blk_size = fatSectorSize;

	bot_data_length = ((uint32_t)cbw.CB[10] << 24) |
			((uint32_t)cbw.CB[11] << 16) |
			((uint32_t)cbw.CB[12] <<  8) |
			(uint32_t)cbw.CB[13];

	for (uint8_t i = 0; i < bot_data_length; ++i) {
		bot_data[i] = 0;
	}

	uint8_t* blk8 = (uint8_t*)&blk_nbr;

	bot_data[4] = blk8[3];
	bot_data[5] = blk8[2];
	bot_data[6] = blk8[1];
	bot_data[7] = blk8[0];

	blk8 = (uint8_t*)&blk_size;

	bot_data[8] = blk8[3];
	bot_data[9] = blk8[2];
	bot_data[10] = blk8[1];
	bot_data[11] = blk8[0];

	return 0;
}


int8_t MSCHandler::SCSI_ModeSense10()
{
	bot_data_length = std::min((uint8_t)sizeof(MSC_Mode_Sense6_data), cbw.CB[8]);
	botBuff = MSC_Mode_Sense10_data;
	return 0;
}


int8_t MSCHandler::SCSI_ModeSense6()
{
	/* ?? Mode page request format
	 * cbw.CB[0] = 0x1A (ModeSense6)
	 * cbw.CB[2] = 0x1C (Page Control (0 = current) | Page Code (0x1C = Informational Exceptions Control))
	 * cbw.CB[3] = 0x00 (Subpage code)
	 * cbw.CB[4] = 0xC0 (Allocation length)
	 */

	bot_data_length = std::min((uint8_t)sizeof(MSC_Mode_Sense6_data), cbw.CB[4]);
	botBuff = MSC_Mode_Sense6_data;
	return 0;
}


int8_t MSCHandler::SCSI_CheckAddressRange(uint32_t blk_offset, uint32_t blk_nbr)
{
	if ((blk_offset + blk_nbr) > fatSectorCount) {
		SCSI_SenseCode(ILLEGAL_REQUEST, ADDRESS_OUT_OF_RANGE);
		return -1;
	}

	return 0;
}


int8_t MSCHandler::SCSI_Read()
{
	if (bot_state == BotState::Idle) {
		if ((cbw.bmFlags & 0x80U) != 0x80U) {
			SCSI_SenseCode(ILLEGAL_REQUEST, INVALID_CDB);
			return -1;
		}

		// Reverse byte order for 32 bit and 16 bit parameters
		scsi_blk_addr = __REV(*(uint32_t*)&(cbw.CB[2]));
		scsi_blk_len = __REVSH(*(uint32_t*)&(cbw.CB[7]));

		if (SCSI_CheckAddressRange(scsi_blk_addr, scsi_blk_len) < 0) {
			return -1;
		}

		if (cbw.dDataLength != (scsi_blk_len * fatSectorSize)) {
			SCSI_SenseCode(ILLEGAL_REQUEST, INVALID_CDB);
			return -1;
		}

		bot_state = BotState::DataIn;
	}

	inBuffSize = std::min(scsi_blk_len * fatSectorSize, MediaPacket);
	inBuffCount = 0;
	csw.dDataResidue -= inBuffSize;

	// Data may be read from cache or flash; if flash (signalled by nullptr), pass a buffer to be filled and wait for DMA transfer to complete
	inBuff = fatTools.GetSectorAddr(scsi_blk_addr, bot_data, inBuffSize);

	if (inBuff != nullptr) {
		ReadReady();
	}

	return 0;
}


void MSCHandler::ReadReady()
{
	// Once read data is ready (may be waiting for DMA) start endpoint transfer
	EndPointTransfer(Direction::in, inEP, inBuffSize);

	scsi_blk_addr += (inBuffSize / fatSectorSize);
	scsi_blk_len -= (inBuffSize / fatSectorSize);

	if (scsi_blk_len == 0) {
		bot_state = BotState::LastDataIn;
	}
}


void MSCHandler::DMATransferDone()
{
	// Triggered once data has been read from flash and copied to local buffer
	SCB_InvalidateDCache_by_Addr((uint32_t*)bot_data, inBuffSize);		// Ensure cache is refreshed after write or erase
	inBuff = bot_data;
	ReadReady();
}


int8_t MSCHandler::SCSI_Write()
{
	if (bot_state == BotState::Idle) {
		if (cbw.dDataLength == 0 || (cbw.bmFlags & 0x80) == 0x80) {
			SCSI_SenseCode(ILLEGAL_REQUEST, INVALID_CDB);
			return -1;
		}

		scsi_blk_addr = __REV(*(uint32_t*)&(cbw.CB[2]));				// Reverse byte order for 32 bit and 16 bit parameters

		if (cbw.CB[0] == SCSI_WRITE10) {
			scsi_blk_len = __REVSH(*(uint16_t*)&(cbw.CB[7]));
		} else {
			scsi_blk_len = __REV(*(uint32_t*)&(cbw.CB[6]));				// Untested - Write16
		}

		if (SCSI_CheckAddressRange(scsi_blk_addr, scsi_blk_len) < 0) {	// check if LBA address is in the right range
			return -1;
		}

		uint32_t len = scsi_blk_len * fatSectorSize;

		if (extFlash.flashCorrupt) {
			SCSI_SenseCode(NOT_READY, WRITE_PROTECTED);
			return -1;
		}

		if (cbw.dDataLength != len) {
			SCSI_SenseCode(ILLEGAL_REQUEST, INVALID_CDB);
			return -1;
		}

		len = std::min(len, MediaPacket);

		// Prepare EP to receive first data packet
		bot_state = BotState::DataOut;
		EndPointTransfer(Direction::out, outEP, len);

	} else {

		// Write Process ongoing
		uint32_t len = std::min(scsi_blk_len * fatSectorSize, MediaPacket);

		fatTools.Write((uint8_t*)(outBuff), scsi_blk_addr, (len / fatSectorSize));

		scsi_blk_addr += (len / fatSectorSize);
		scsi_blk_len -= (len / fatSectorSize);
		csw.dDataResidue -= len;			// case 12 : Ho = Do

		if (scsi_blk_len == 0)	{
			MSC_BOT_SendCSW(CSWCmdPassed);
		} else {
			len = std::min((scsi_blk_len * fatSectorSize), MediaPacket);
			EndPointTransfer(Direction::out, outEP, len);				// Prepare EP to Receive next packet
		}
	}

	return 0;
}


int8_t MSCHandler::SCSI_TestUnitReady()
{
	// Tests if the storage device is ready to receive commands; called continuously in Windows every second or so
	if (cbw.dDataLength != 0) {
		SCSI_SenseCode(ILLEGAL_REQUEST, INVALID_CDB);
		return -1;
	}

//	if (extFlash.flashCorrupt || fatTools.noFileSystem) {
//		SCSI_SenseCode(HARDWARE_ERROR, MEDIUM_NOT_PRESENT);
//		return -1;
//	}

	bot_data_length = 0;
	return 0;
}


int8_t MSCHandler::SCSI_AllowPreventRemovable()
{
	if (cbw.CB[4] == 0) {
		scsi_medium_state = SCSI_MEDIUM_UNLOCKED;
	} else {
		scsi_medium_state = SCSI_MEDIUM_LOCKED;
	}

	bot_data_length = 0;
	return 0;
}


int8_t MSCHandler::SCSI_Verify10()			// Untested
{
	if ((cbw.CB[1] & 0x02U) == 0x02U) {
		SCSI_SenseCode(ILLEGAL_REQUEST, INVALID_FIELD_IN_COMMAND);
		return -1;
	}

	if (SCSI_CheckAddressRange(scsi_blk_addr, scsi_blk_len) < 0) {
		return -1;
	}

	bot_data_length = 0;

	return 0;
}


void MSCHandler::SCSI_SenseCode(uint8_t sKey, uint8_t ASC)
{
	scsi_sense[scsi_sense_tail].Skey = sKey;
	scsi_sense[scsi_sense_tail].ASC = ASC;
	scsi_sense[scsi_sense_tail].ASCQ = 0;
	scsi_sense_tail++;

	if (scsi_sense_tail == SenseListDepth) {
		scsi_sense_tail = 0;
	}
}


int8_t MSCHandler::SCSI_RequestSense()
{
	if (cbw.dDataLength == 0U) {
		SCSI_SenseCode(ILLEGAL_REQUEST, INVALID_CDB);
		return -1;
	}

	for (uint8_t i = 0; i < REQUEST_SENSE_DATA_LEN; ++i) {
		bot_data[i] = 0;
	}

	bot_data[0] = 0x70;
	bot_data[7] = REQUEST_SENSE_DATA_LEN - 6;

	if (scsi_sense_head != scsi_sense_tail) {
		bot_data[2]  = scsi_sense[scsi_sense_head].Skey;
		bot_data[12] = scsi_sense[scsi_sense_head].ASC;
		bot_data[13] = scsi_sense[scsi_sense_head].ASCQ;
		scsi_sense_head++;

		if (scsi_sense_head == SenseListDepth) {
			scsi_sense_head = 0;
		}
	}

	bot_data_length = std::min((uint8_t)REQUEST_SENSE_DATA_LEN, cbw.CB[4]);

	return 0;
}



// Descriptor definition here as requires constants from USB class
const uint8_t MSCHandler::Descriptor[] = {
	//--------------------------------------------------------------------------------------
	// MSC Descriptor
	0x09,								// sizeof(usbDescrInterface): length of descriptor in bytes
	USB::InterfaceDescriptor,			// interface descriptor type
	USB::MSCInterface,					// index of this interface
	0x00,								// alternate setting for this interface
	0x02,								// endpoints excl 0: number of endpoint descriptors to follow
	0x08,								// Mass Storage
	0x06,								// SCSI transparent command set
	0x50,								// bInterfaceProtocol: Bulk-Only Transport
	USB::MassStorageClass,				// string index for interface

	// Bulk IN Endpoint Descriptor
	0x07,								// bLength
	USB::EndpointDescriptor,			// bDescriptorType = endpoint
	USB::MSC_In,						// bEndpointAddress IN endpoint number 3
	USB::Bulk,							// bmAttributes: 2: Bulk, 3: Interrupt endpoint
	LOBYTE(USB::ep_maxPacket),			// wMaxPacketSize
	HIBYTE(USB::ep_maxPacket),
	0x00,								// bInterval in ms

	// Bulk OUT Endpoint Descriptor
	0x07,								// bLength
	USB::EndpointDescriptor,			// bDescriptorType = endpoint
	USB::MSC_Out,						// bEndpointAddress
	USB::Bulk,							// bmAttributes: 2:Bulk
	LOBYTE(USB::ep_maxPacket),			// wMaxPacketSize
	HIBYTE(USB::ep_maxPacket),
	0x00,								// bInterval in ms : ignored for bulk
};

uint32_t MSCHandler::GetInterfaceDescriptor(const uint8_t** buffer) {
	*buffer = Descriptor;
	return sizeof(Descriptor);
}
