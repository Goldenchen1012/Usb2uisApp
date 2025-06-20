#include "usb2uis_interface.h"

QLibrary Usb2UisInterface::m_lib;

BYTE (__stdcall *Usb2UisInterface::pOpenDevice)(void) = nullptr;
bool (__stdcall *Usb2UisInterface::pCloseDevice)(BYTE) = nullptr;
bool (__stdcall *Usb2UisInterface::pSPISetConfig)(BYTE, BYTE, DWORD) = nullptr;
bool (__stdcall *Usb2UisInterface::pSPIRead)(BYTE, BYTE*, BYTE, BYTE*, WORD) = nullptr;
bool (__stdcall *Usb2UisInterface::pSPIWrite)(BYTE, BYTE*, BYTE, BYTE*, WORD) = nullptr;
bool (__stdcall *Usb2UisInterface::pSetCE)(BYTE, bool) = nullptr;

bool  (__stdcall *Usb2UisInterface::pGetGPIOCfg)   (BYTE,BYTE*)           = nullptr;
bool  (__stdcall *Usb2UisInterface::pSetGPIOCfg)   (BYTE,BYTE)            = nullptr;
bool  (__stdcall *Usb2UisInterface::pGPIORead)     (BYTE,BYTE*)           = nullptr;
bool  (__stdcall *Usb2UisInterface::pGPIOWrite)    (BYTE,BYTE,BYTE)       = nullptr;

bool Usb2UisInterface::init(const QString &dllPath)
{
    m_lib.setFileName(dllPath);
    if (!m_lib.load()) return false;

    pOpenDevice   = (BYTE (__stdcall*)()) m_lib.resolve("USBIO_OpenDevice");
    pCloseDevice  = (bool (__stdcall*)(BYTE)) m_lib.resolve("USBIO_CloseDevice");
    pSPISetConfig = (bool (__stdcall*)(BYTE, BYTE, DWORD)) m_lib.resolve("USBIO_SPISetConfig");
    pSPIRead      = (bool (__stdcall*)(BYTE, BYTE*, BYTE, BYTE*, WORD)) m_lib.resolve("USBIO_SPIRead");
    pSPIWrite     = (bool (__stdcall*)(BYTE, BYTE*, BYTE, BYTE*, WORD)) m_lib.resolve("USBIO_SPIWrite");

    pSetCE = (bool (__stdcall*)(BYTE, bool)) m_lib.resolve("USBIO_SetCE");

    pGetGPIOCfg    = (bool (__stdcall*)(BYTE,BYTE*))          m_lib.resolve("USBIO_GetGPIOConfig");
    pSetGPIOCfg    = (bool (__stdcall*)(BYTE,BYTE))           m_lib.resolve("USBIO_SetGPIOConfig");
    pGPIORead      = (bool (__stdcall*)(BYTE,BYTE*))          m_lib.resolve("USBIO_GPIORead");
    pGPIOWrite     = (bool (__stdcall*)(BYTE,BYTE,BYTE))      m_lib.resolve("USBIO_GPIOWrite");

    return(pOpenDevice && pCloseDevice && pSPISetConfig && pSPIRead && pSPIWrite && pSetCE &&
           pGetGPIOCfg && pSetGPIOCfg && pGPIORead && pGPIOWrite);
}

BYTE Usb2UisInterface::USBIO_OpenDevice() {
    return pOpenDevice ? pOpenDevice() : 0xFF;
}

bool Usb2UisInterface::USBIO_CloseDevice(BYTE index) {
    return pCloseDevice && pCloseDevice(index);
}

bool Usb2UisInterface::USBIO_SPISetConfig(BYTE index, BYTE rate, DWORD timeout) {
    return pSPISetConfig && pSPISetConfig(index, rate, timeout);
}

bool Usb2UisInterface::USBIO_SPIRead(BYTE index, BYTE* cmd, BYTE cmdSize, BYTE* buffer, WORD size) {
    return pSPIRead && pSPIRead(index, cmd, cmdSize, buffer, size);
}

bool Usb2UisInterface::USBIO_SPIWrite(BYTE index, BYTE* cmd, BYTE cmdSize, BYTE* buffer, WORD size) {
    return pSPIWrite && pSPIWrite(index, cmd, cmdSize, buffer, size);
}

bool Usb2UisInterface::USBIO_SetCE(BYTE index, bool high) {
    return pSetCE && pSetCE(index, high);
}

bool Usb2UisInterface::USBIO_GetGPIOConfig(BYTE i,BYTE *d){
    return pGetGPIOCfg && pGetGPIOCfg(i,d);
}

bool Usb2UisInterface::USBIO_SetGPIOConfig(BYTE i,BYTE  d){
    return pSetGPIOCfg && pSetGPIOCfg(i,d);
}

bool Usb2UisInterface::USBIO_GPIORead(BYTE i,BYTE *v){
    return pGPIORead   && pGPIORead(i,v);
}

bool Usb2UisInterface::USBIO_GPIOWrite(BYTE i,BYTE  v,BYTE m){
    return pGPIOWrite  && pGPIOWrite(i,v,m);
}
