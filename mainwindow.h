#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QByteArray>
#include <QEventLoop>
#include <QTimer>
#include <QElapsedTimer>
#include <QList>
#include <QMap>
#include <QStringListModel>
#include "usb2uis_interface.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE


typedef  enum{
    USB2UIS_GPIO_IO1 = 0,
    USB2UIS_GPIO_IO2,
    USB2UIS_GPIO_IO3,
    USB2UIS_GPIO_IO4,
    USB2UIS_GPIO_IO5,
    USB2UIS_GPIO_IO6,
    USB2UIS_GPIO_IO7,
    USB2UIS_GPIO_IO8,
}eTypeGPIO_IO_PORT;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnConnect_clicked();
    void on_btnApplyConfig_clicked();
    void on_btnSpiRead_clicked();
    void on_btnSpiRead2_clicked();
    void on_btnSpiWrite_clicked();
    void on_btnClearResult_clicked();
    // 按鈕
    void on_btnLoadReadCmdList_clicked();
    void on_btnLoadWriteCmdList_clicked();
    void on_btnLoadWriteDataList_clicked();

    // ComboBox 選擇
    void onReadCmdChosen(const QString &);
    void onWriteCmdChosen(const QString &);
    void onWriteDataChosen(const QString &);
    void on_btnLoadReadCmdSet_clicked();
    void on_comboReadCmdSet_currentIndexChanged(int index);

    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

    void on_rdoNorth_clicked();

    void on_rdoSouth_clicked();

private:
    Ui::MainWindow *ui;

    bool deviceConnected = false;
    BYTE deviceIndex = 0xFF;
    bool bDirNorth = true;

    bool parseHexString(const QString& input, QByteArray& output);
    void delayBlockingMs(int ms);
    void delayBlockingUs(int usec);
    void loadReadCmdSet();
    void loadReadCmdList();
    void GpioSet(eTypeGPIO_IO_PORT eGpio);
    void GpioClear(eTypeGPIO_IO_PORT eGpio);
    void SpiDirectionHighLow(bool bDirNorth, bool bHigh);

    QMap<int, QString> mapSetDescription;             // SET編號 → 註解
    QList<QPair<int, QString>> listSetCmdsOrdered;    // 保留原始順序, SET編號 → HEX字串
    QStringListModel *cmdListModel = nullptr;         // ListView 模型
};
#endif // MAINWINDOW_H
