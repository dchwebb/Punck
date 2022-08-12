#include <USBHandler.h>
#include "USB.h"

USBHandler::USBHandler(USB* usb, uint8_t inEP, uint8_t outEP, int8_t interface) : usb(usb), inEP(inEP), outEP(outEP), interface(interface) {
	usb->classesByInterface[interface] = this;
	usb->classbyEP[outEP] = this;
}


void USBHandler::EndPointTransfer(Direction d, uint8_t ep, uint32_t len)
{
   usb->EPStartXfer(d, ep, len);
}

void USBHandler::SetupIn(uint32_t size, const uint8_t* buff)
{
	usb->EP0In(buff, size);
}



void EP0Handler::DataIn()
{

}

void EP0Handler::DataOut()
{

}

void EP0Handler::ClassSetup(usbRequest& req)
{

}

void EP0Handler::ClassSetupData(usbRequest& req, const uint8_t* data)
{

}
