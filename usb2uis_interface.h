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

    /*
     * USBIO_GetGPIOConfig  取得 GPIO 方向 (1=input, 0=output)
     * USBIO_SetGPIOConfig  設定 GPIO 方向
     * USBIO_GPIORead       讀取 GPIO 電平  (1=High, 0=Low)
     * USBIO_GPIOWrite      寫入 GPIO 電平  (Maskbit=1 如果該位元為 1，則only read 僅讀取對應的引腳)
     */
    static bool USBIO_GetGPIOConfig(BYTE index, BYTE *dirByte);
    static bool USBIO_SetGPIOConfig(BYTE index, BYTE  dirByte);
    static bool USBIO_GPIORead     (BYTE index, BYTE *valueByte);
    static bool USBIO_GPIOWrite    (BYTE index, BYTE  valueByte, BYTE  maskByte);

private:
    static QLibrary m_lib;

    static BYTE (__stdcall *pOpenDevice)(void);
    static bool (__stdcall *pCloseDevice)(BYTE);
    static bool (__stdcall *pSPISetConfig)(BYTE, BYTE, DWORD);
    static bool (__stdcall *pSPIRead)(BYTE, BYTE*, BYTE, BYTE*, WORD);
    static bool (__stdcall *pSPIWrite)(BYTE, BYTE*, BYTE, BYTE*, WORD);
    static bool (__stdcall *pSetCE)(BYTE, bool);

    static bool (__stdcall *pGetGPIOCfg)    (BYTE,BYTE*);
    static bool (__stdcall *pSetGPIOCfg)    (BYTE,BYTE);
    static bool (__stdcall *pGPIORead)      (BYTE,BYTE*);
    static bool (__stdcall *pGPIOWrite)     (BYTE,BYTE,BYTE);
};

#endif // USB2UIS_INTERFACE_H
