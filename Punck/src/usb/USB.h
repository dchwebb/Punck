#pragma once

#include "initialisation.h"
#include "USBHandler.h"
#include "MSCHandler.h"
#include "CDCHandler.h"
#include "MidiHandler.h"
#include <functional>
#include <cstring>

// Enables capturing of debug data for output over STLink UART on dev boards
#define USB_DEBUG false
#if (USB_DEBUG)
#include "uartHandler.h"
#define USB_DEBUG_COUNT 1000
#endif


// USB Hardware Registers
#define USBx_PCGCCTL	*(__IO uint32_t *)(USB_OTG_FS_PERIPH_BASE + USB_OTG_PCGCCTL_BASE)
#define USBx_DEVICE		((USB_OTG_DeviceTypeDef *)(USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE))
#define USBx_INEP(i)	((USB_OTG_INEndpointTypeDef *)(USB_OTG_FS_PERIPH_BASE + USB_OTG_IN_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))
#define USBx_OUTEP(i)	((USB_OTG_OUTEndpointTypeDef *)(USB_OTG_FS_PERIPH_BASE + USB_OTG_OUT_ENDPOINT_BASE + ((i) * USB_OTG_EP_REG_SIZE)))
#define USBx_DFIFO(i)	*(uint32_t*)(USB_OTG_FS_PERIPH_BASE + USB_OTG_FIFO_BASE + ((i) * USB_OTG_FIFO_SIZE))

#define EP_ADDR_MASK					0xFU
#define USB_REQ_DIRECTION_MASK			0x80U
#define USB_REQ_TYPE_MASK				0x60U

#define LOBYTE(x)  ((uint8_t)(x & 0x00FFU))
#define HIBYTE(x)  ((uint8_t)((x & 0xFF00U) >> 8U))


class USB {
	friend class USBHandler;
public:
	enum Interface {NoInterface = -1, AudioInterface = 0, MidiInterface = 1, MSCInterface = 2, CDCCmdInterface = 3, CDCDataInterface = 4, interfaceCount = 5};
	enum EndPoint {Midi_In = 0x81, Midi_Out = 0x1, MSC_In = 0x82, MSC_Out = 0x2, CDC_In = 0x83, CDC_Out = 0x3, CDC_Cmd = 0x84};
	enum EndPointType {Control = 0, Isochronous = 1, Bulk = 2, Interrupt = 3};
	enum class DeviceState {Default, Addressed, Configured, Suspended};
	enum RequestRecipient {RequestRecipientDevice = 0x0, RequestRecipientInterface = 0x1, RequestRecipientEndpoint = 0x2};
	enum RequestType {RequestTypeStandard = 0x0, RequestTypeClass = 0x20, RequestTypeVendor = 0x40};
	enum class Request {GetStatus = 0x0, SetAddress = 0x5, GetDescriptor = 0x6, SetConfiguration = 0x9};
	enum Descriptor {DeviceDescriptor = 0x1, ConfigurationDescriptor = 0x2, StringDescriptor = 0x3, InterfaceDescriptor = 0x4, EndpointDescriptor = 0x5, DeviceQualifierDescriptor = 0x6, IadDescriptor = 0xb, BosDescriptor = 0xF, ClassSpecificInterfaceDescriptor = 0x24};
	enum PacketStatus {GlobalOutNAK = 1, OutDataReceived = 2, OutTransferCompleted = 3, SetupTransComplete = 4, SetupDataReceived = 6};
	static constexpr uint8_t ep_maxPacket = 0x40;

	void InterruptHandler();
	void Init();
	size_t SendData(const uint8_t *data, uint16_t len, uint8_t endpoint);
	void SendString(const char* s);
	void SendString(const std::string s);
	size_t SendString(const unsigned char* s, size_t len);
	void PauseEndpoint(USBHandler& handler);
	void ResumeEndpoint(USBHandler& handler);
	uint32_t StringToUnicode(const std::string_view desc, uint8_t* unicode);

	EP0Handler  ep0  = EP0Handler(this, 0, 0, NoInterface);
	MidiHandler midi = MidiHandler(this, USB::Midi_In, USB::Midi_Out, MidiInterface);
	MSCHandler  msc  = MSCHandler(this,  USB::MSC_In,  USB::MSC_Out,  MSCInterface);
	CDCHandler  cdc  = CDCHandler(this,  USB::CDC_In,  USB::CDC_Out,  CDCCmdInterface);
	bool classPendingData = false;			// Set when class setup command received and data pending
	DeviceState devState;

private:
	enum class EP0State {Idle, Setup, DataIn, DataOut, StatusIn, StatusOut, Stall};
	enum StringIndex {LangId = 0, Manufacturer = 1, Product = 2, Serial = 3, Configuration = 4, MassStorageClass = 5,
		CommunicationClass = 6, AudioClass = 7};
	static constexpr std::string_view USBD_MANUFACTURER_STRING = "Mountjoy Modular";
	static constexpr std::string_view USBD_PRODUCT_STRING = "Mountjoy Punck MSC";
	static constexpr std::string_view USBD_CFG_STRING = "MSC Punck Config";
	static constexpr std::string_view USBD_MSC_STRING = "MSC Punck Interface";
	static constexpr std::string_view USBD_CDC_STRING = "Mountjoy Punck CDC";
	static constexpr std::string_view USBD_MIDI_STRING	= "Mountjoy Punck MIDI";
	static constexpr uint8_t usbSerialNoSize = 24;


	void ActivateEndpoint(uint8_t endpoint, const Direction direction, const EndPointType eptype);
	void ReadPacket(const uint32_t* dest, uint16_t len, uint32_t offset);
	void WritePacket(const uint8_t* src, uint8_t endpoint, uint32_t len);
	void GetDescriptor();
	void StdDevReq();
	void EPStartXfer(const Direction direction, uint8_t endpoint, uint32_t xfer_len);
	void EP0In(const uint8_t* buff, uint32_t size);
	void CtlError();
	bool ReadInterrupts(const uint32_t interrupt);
	void SerialToUnicode();

	std::array<USBHandler*, 4>classesByInterface;		// Lookup tables to get appropriate class handlers
	std::array<USBHandler*, 4>classByEP;
	EP0State ep0State;
	bool transmitting;
	usbRequest req;
	uint8_t USBD_StrDesc[128];


	// USB standard device descriptor
	static constexpr uint16_t VendorID = 1155;	// STMicroelectronics
	static constexpr uint16_t ProductId = 0x572c;

	const uint8_t USBD_FS_DeviceDesc[0x12] = {
			0x12,								// bLength
			DeviceDescriptor,					// bDescriptorType
			0x01,								// bcdUSB  - 0x01 if LPM enabled
			0x02,
			0xEF,								// bDeviceClass: (Miscellaneous)
			0x02,								// bDeviceSubClass (Interface Association Descriptor- with below)
			0x01,								// bDeviceProtocol (Interface Association Descriptor)
			ep_maxPacket,  						// bMaxPacketSize
			LOBYTE(VendorID),					// idVendor
			HIBYTE(VendorID),					// idVendor
			LOBYTE(ProductId),					// idProduct
			HIBYTE(ProductId),					// idProduct
			0x00,								// bcdDevice rel. 2.00
			0x02,
			StringIndex::Manufacturer,			// Index of manufacturer  string
			StringIndex::Product,				// Index of product string
			StringIndex::Serial,				// Index of serial number string
			0x01								// bNumConfigurations
	};

	// Configuration Descriptor
	static constexpr uint8_t configDescSize = 175;
	static constexpr uint8_t MidiClassDescSize = 50;

	const uint8_t ConfigDesc[configDescSize] = {
			0x09,								// bLength: Configuration Descriptor size
			ConfigurationDescriptor,			// bDescriptorType: Configuration
			LOBYTE(configDescSize),				// wTotalLength
			HIBYTE(configDescSize),
			interfaceCount,						// bNumInterfaces: 5 [1 MSC, 2 CDC, 2 MIDI]
			0x01,								// bConfigurationValue: Configuration value
			0x04,								// iConfiguration: Index of string descriptor describing the configuration
			0xC0,								// bmAttributes: self powered
			0x32,								// MaxPower 0 mA


			//---------------------------------------------------------------------------
			// MIDI Descriptors

			// B.3.1 Standard Audio Control standard Interface Descriptor
			0x09,								// sizeof(usbDescrInterface): length of descriptor in bytes
			InterfaceDescriptor,				// interface descriptor type
			AudioInterface,						// index of this interface
			0x00,								// alternate setting for this interface
			0x00,								// endpoints excl 0: number of endpoint descriptors to follow
			0x01,								// AUDIO
			0x01,								// AUDIO_Control
			0x00,								// bInterfaceProtocol
			StringIndex::AudioClass,			// string index for interface

			// B.3.2 Class-specific AC Interface Descriptor
			0x09,								// sizeof(usbDescrCDC_HeaderFn): length of descriptor in bytes
			ClassSpecificInterfaceDescriptor,	// descriptor type
			0x01,								// header functional descriptor
			0x00, 0x01,							// bcdADC
			0x09, 0x00,							// wTotalLength
			0x01,								// bInCollection
			0x01,								// baInterfaceNr[1]

			// B.4 MIDIStreaming Interface Descriptors
			// B.4.1 Standard MS Interface Descriptor
			0x09,								// bLength
			InterfaceDescriptor,				// bDescriptorType: interface descriptor
			MidiInterface,						// bInterfaceNumber
			0x00,								// bAlternateSetting
			0x02,								// bNumEndpoints
			0x01,								// bInterfaceClass: Audio
			0x03,								// bInterfaceSubClass: MIDIStreaming
			0x00,								// InterfaceProtocol
			StringIndex::AudioClass,			// iInterface: String Descriptor

			// B.4.2 Class-specific MS Interface Descriptor
			0x07,								// length of descriptor in bytes
			ClassSpecificInterfaceDescriptor,	// bDescriptorType: Class Specific Interface Descriptor
			0x01,								// header functional descriptor
			0x0, 0x01,							// bcdADC
			MidiClassDescSize, 0,				// wTotalLength

			// B.4.3 MIDI IN Jack Descriptor (Embedded)
			0x06,								// bLength
			ClassSpecificInterfaceDescriptor,	// descriptor type
			0x02,								// bDescriptorSubtype: MIDI_IN_JACK
			0x01,								// bJackType: Embedded
			0x01,								// bJackID
			0x00,								// iJack: No String Descriptor

			// Table B4.4 Midi Out Jack Descriptor (Embedded)
			0x09,								// length of descriptor in bytes
			ClassSpecificInterfaceDescriptor,	// descriptor type
			0x03,								// MIDI_OUT_JACK descriptor
			0x01,								// bJackType: Embedded
			0x02,								// bJackID
			0x01,								// No of input pins
			0x01,								// ID of the Entity to which this Pin is connected.
			0x01,								// Output Pin number of the Entity to which this Input Pin is connected.
			0x00,								// iJack

			//B.5.1 Standard Bulk OUT Endpoint Descriptor
			0x09,								// bLength
			EndpointDescriptor,					// bDescriptorType = endpoint
			Midi_Out,							// bEndpointAddress
			Bulk,								// bmAttributes: 2:Bulk
			LOBYTE(ep_maxPacket),				// wMaxPacketSize
			HIBYTE(ep_maxPacket),
			0x00,								// bInterval in ms : ignored for bulk
			0x00,								// bRefresh Unused
			0x00,								// bSyncAddress Unused

			// B.5.2 Class-specific MS Bulk OUT Endpoint Descriptor
			0x05,								// bLength of descriptor in bytes
			0x25,								// bDescriptorType (Audio Endpoint Descriptor)
			0x01,								// bDescriptorSubtype: MS General
			0x01,								// bNumEmbMIDIJack
			0x01,								// baAssocJackID (0) ID of the Embedded MIDI IN Jack.

			//B.6.1 Standard Bulk IN Endpoint Descriptor
			0x09,								// bLength
			EndpointDescriptor,					// bDescriptorType = endpoint
			Midi_In,							// bEndpointAddress IN endpoint number 3
			Bulk,								// bmAttributes: 2: Bulk, 3: Interrupt endpoint
			LOBYTE(ep_maxPacket),				// wMaxPacketSize
			HIBYTE(ep_maxPacket),
			0x00,								// bInterval in ms
			0x00,								// bRefresh
			0x00,								// bSyncAddress

			// B.6.2 Class-specific MS Bulk IN Endpoint Descriptor
			0x05,								// bLength of descriptor in bytes
			0x25,								// bDescriptorType
			0x01,								// bDescriptorSubtype
			0x01,								// bNumEmbMIDIJack (0)
			0x02,								// baAssocJackID (0) ID of the Embedded MIDI OUT Jack


			//--------------------------------------------------------------------------------------
			// MSC Descriptor
			0x09,								// sizeof(usbDescrInterface): length of descriptor in bytes
			InterfaceDescriptor,				// interface descriptor type
			MSCInterface,						// index of this interface
			0x00,								// alternate setting for this interface
			0x02,								// endpoints excl 0: number of endpoint descriptors to follow
			0x08,								// Mass Storage
			0x06,								// SCSI transparent command set
			0x50,								// bInterfaceProtocol: Bulk-Only Transport
			StringIndex::MassStorageClass,		// string index for interface

			// Bulk IN Endpoint Descriptor
			0x07,								// bLength
			EndpointDescriptor,					// bDescriptorType = endpoint
			MSC_In,								// bEndpointAddress IN endpoint number 3
			Bulk,								// bmAttributes: 2: Bulk, 3: Interrupt endpoint
			LOBYTE(ep_maxPacket),				// wMaxPacketSize
			HIBYTE(ep_maxPacket),
			0x00,								// bInterval in ms

			// Bulk OUT Endpoint Descriptor
			0x07,								// bLength
			EndpointDescriptor,					// bDescriptorType = endpoint
			MSC_Out,							// bEndpointAddress
			Bulk,								// bmAttributes: 2:Bulk
			LOBYTE(ep_maxPacket),				// wMaxPacketSize
			HIBYTE(ep_maxPacket),
			0x00,								// bInterval in ms : ignored for bulk


			//---------------------------------------------------------------------------
	        // IAD Descriptor - Interface association descriptor for CDC class
			0x08,								// bLength (8 bytes)
			IadDescriptor,						// bDescriptorType
			CDCCmdInterface,					// bFirstInterface
			0x02,								// bInterfaceCount
			0x02,								// bFunctionClass (Communications and CDC Control)
			0x02,								// bFunctionSubClass
			0x01,								// bFunctionProtocol
			StringIndex::CommunicationClass,	// String Descriptor

			// Interface Descriptor
			0x09,								// bLength: Interface Descriptor size
			InterfaceDescriptor,				// bDescriptorType: Interface
			CDCCmdInterface,					// bInterfaceNumber: Number of Interface
			0x00,								// bAlternateSetting: Alternate setting
			0x01,								// bNumEndpoints: 1 endpoint used
			0x02,								// bInterfaceClass: Communication Interface Class
			0x02,								// bInterfaceSubClass: Abstract Control Model
			0x01,								// bInterfaceProtocol: Common AT commands
			StringIndex::CommunicationClass,	// iInterface

			// Header Functional Descriptor
			0x05,								// bLength: Endpoint Descriptor size
			ClassSpecificInterfaceDescriptor,	// bDescriptorType: CS_INTERFACE
			0x00,								// bDescriptorSubtype: Header Func Desc
			0x10,								// bcdCDC: spec release number
			0x01,

			// Call Management Functional Descriptor
			0x05,								// bFunctionLength
			ClassSpecificInterfaceDescriptor,	// bDescriptorType: CS_INTERFACE
			0x01,								// bDescriptorSubtype: Call Management Func Desc
			0x00,								// bmCapabilities: D0+D1
			0x01,								// bDataInterface: 1

			// ACM Functional Descriptor
			0x04,								// bFunctionLength
			ClassSpecificInterfaceDescriptor,	// bDescriptorType: CS_INTERFACE
			0x02,								// bDescriptorSubtype: Abstract Control Management desc
			0x02,								// bmCapabilities

			// Union Functional Descriptor
			0x05,								// bFunctionLength
			ClassSpecificInterfaceDescriptor,	// bDescriptorType: CS_INTERFACE
			0x06,								// bDescriptorSubtype: Union func desc
			0x00,								// bMasterInterface: Communication class interface FIXME
			0x01,								// bSlaveInterface0: Data Class Interface

			// Endpoint 2 Descriptor
			0x07,								// bLength: Endpoint Descriptor size
			EndpointDescriptor,					// bDescriptorType: Endpoint
			CDC_Cmd,							// bEndpointAddress
			Interrupt,							// bmAttributes: Interrupt
			0x08,								// wMaxPacketSize
			0x00,
			0x10,								// bInterval

			//---------------------------------------------------------------------------

			// Data class interface descriptor
			0x09,								// bLength: Endpoint Descriptor size
			InterfaceDescriptor,				// bDescriptorType:
			CDCDataInterface,					// bInterfaceNumber: Number of Interface
			0x00,								// bAlternateSetting: Alternate setting
			0x02,								// bNumEndpoints: Two endpoints used
			0x0A,								// bInterfaceClass: CDC
			0x00,								// bInterfaceSubClass:
			0x00,								// bInterfaceProtocol:
			0x00,								// iInterface:

			// Endpoint OUT Descriptor
			0x07,								// bLength: Endpoint Descriptor size
			EndpointDescriptor,					// bDescriptorType: Endpoint
			CDC_Out,							// bEndpointAddress
			Bulk,								// bmAttributes: Bulk
			LOBYTE(ep_maxPacket),				// wMaxPacketSize:
			HIBYTE(ep_maxPacket),
			0x00,								// bInterval: ignore for Bulk transfer

			// Endpoint IN Descriptor
			0x07,								// bLength: Endpoint Descriptor size
			EndpointDescriptor,					// bDescriptorType: Endpoint
			CDC_In,								// bEndpointAddress
			Bulk,								// bmAttributes: Bulk
			LOBYTE(ep_maxPacket),				// wMaxPacketSize:
			HIBYTE(ep_maxPacket),
			0x00,								// bInterval: ignore for Bulk transfer
	};

	// Binary Object Store (BOS) Descriptor
	const uint8_t USBD_FS_BOSDesc[12] = {
			0x05,								// Length
			BosDescriptor,						// DescriptorType
			0x0C,								// TotalLength
			0x00, 0x01,							// NumDeviceCaps

			// USB 2.0 Extension Descriptor: device capability
			0x07,								// bLength
			0x10, 								// USB_DEVICE_CAPABITY_TYPE
			0x02,								// Attributes
			0x02, 0x00, 0x00, 0x00				// Link Power Management protocol is supported
	};


	// USB language indentifier descriptor
	static constexpr uint16_t languageIDString = 1033;

	const uint8_t USBD_LangIDDesc[4] = {
			4,
			StringDescriptor,
			LOBYTE(languageIDString),
			HIBYTE(languageIDString)
	};

	const uint8_t USBD_MSC_DeviceQualifierDesc[10] = {
			10,
			DeviceQualifierDescriptor,
			0x00,
			0x02,
			0x01,
			0x00,
			0x00,
			0x40,
			0x01,
			0x00,
	};


public:
#if (USB_DEBUG)
	uint16_t usbDebugNo = 0;
	uint16_t usbDebugEvent = 0;

	struct usbDebugItem {
		uint16_t eventNo;
		uint32_t Interrupt;
		uint32_t IntData;
		usbRequest Request;
		uint8_t endpoint;
		uint8_t scsiOpCode;
		uint16_t PacketSize;
		uint32_t xferBuff[4];
	};
	usbDebugItem usbDebug[USB_DEBUG_COUNT];
	void OutputDebug();

	// Update the current debug record
	void USBUpdateDbg(uint32_t IntData, usbRequest request, uint8_t endpoint, uint16_t PacketSize, uint32_t scsiOpCode, uint32_t* xferBuff)
	{
		if (usbDebugNo < USB_DEBUG_COUNT - 1) {
			if (IntData) usbDebug[usbDebugNo].IntData = IntData;
			if (((uint32_t*)&request)[0]) usbDebug[usbDebugNo].Request = request;
			if (endpoint) usbDebug[usbDebugNo].endpoint = endpoint;
			if (PacketSize) usbDebug[usbDebugNo].PacketSize = PacketSize;
			if (scsiOpCode) usbDebug[usbDebugNo].scsiOpCode = scsiOpCode;
			if (xferBuff != nullptr) {
				usbDebug[usbDebugNo].xferBuff[0] = xferBuff[0];
				usbDebug[usbDebugNo].xferBuff[1] = xferBuff[1];
				usbDebug[usbDebugNo].xferBuff[2] = xferBuff[2];
				usbDebug[usbDebugNo].xferBuff[3] = xferBuff[3];
			}
		}
	}
#else
#define USBUpdateDbg(IntData, request, endpoint, PacketSize, scsiOpCode, xferBuff)
#endif

};

extern USB usb;
