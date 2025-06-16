#ifndef USB2UIS_INTERFACE_H
#define USB2UIS_INTERFACE_H

#include <QLibrary>
#include <QString>

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned int long DWORD;

class Usb2UisInterface {
public:
    static bool init(const QString &dllPath = "usb2uis.dll");

    static BYTE USBIO_OpenDevice();
    static bool USBIO_CloseDevice(BYTE index);
    static bool USBIO_SPISetConfig(BYTE index, BYTE rate, DWORD timeout);
    static bool USBIO_SPIRead(BYTE index, BYTE* cmd, BYTE cmdSize, BYTE* buffer, WORD size);
    static bool USBIO_SPIWrite(BYTE index, BYTE* cmd, BYTE cmdSize, BYTE* buffer, WORD size);
    static bool USBIO_SetCE(BYTE index, bool high);

private:
    static QLibrary m_lib;

    static BYTE (__stdcall *pOpenDevice)(void);
    static bool (__stdcall *pCloseDevice)(BYTE);
    static bool (__stdcall *pSPISetConfig)(BYTE, BYTE, DWORD);
    static bool (__stdcall *pSPIRead)(BYTE, BYTE*, BYTE, BYTE*, WORD);
    static bool (__stdcall *pSPIWrite)(BYTE, BYTE*, BYTE, BYTE*, WORD);
    static bool (__stdcall *pSetCE)(BYTE, bool);
};

#endif // USB2UIS_INTERFACE_H
