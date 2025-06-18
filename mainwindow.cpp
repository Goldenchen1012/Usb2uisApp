#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "usb2uis_interface.h"

#include <QMessageBox>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <QTime>

#define EMULATOR_APP_NAME_STR         QString("Usb2uisApp")
#define EMULATOR_APP_VERSION_STR      QString("V1.0")

BYTE deviceIndex = 0;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle(EMULATOR_APP_NAME_STR + " " + EMULATOR_APP_VERSION_STR);

    // 初始化DLL
    if (!Usb2UisInterface::init()) {
        QMessageBox::critical(this, "錯誤", "無法載入 usb2uis.dll");
    }

    // 初始化界面元件
    ui->comboSpiSpeed->addItems({"200KHz", "400KHz", "600KHz", "800KHz", "1MHz", "2MHz", "4MHz", "6MHz", "12MHz"});
    ui->comboSpiMode->addItems({"Mode0 (00)", "Mode1 (01)", "Mode2 (10)", "Mode3 (11)"});

    // 預設 timeout 值
    ui->lineReadTimeout->setText("100");
    ui->lineWriteTimeout->setText("100");
    ui->lineSpiDelayMs->setText("0");     //Base delay time= 56us + Set(ms)

    // 預設顯示第1個分頁（索引從0開始）
    ui->tabWidget->setCurrentIndex(0);

    //For Test
    //--------------------------------------
    ui->lineReadCmd->setText("0x00 0x02 0x2B 0x0A");
    ui->lineWriteCmd->setText("0x00 0x01 0x3D 0x6E");
    ui->lineReadBytes->setText("8");
    ui->textWriteData->setPlainText("0x81 0x00 0x00 0xFF 0x03 0x00 0x02 0x8E");
    //--------------------------------------


    ui->lineDummyCount->setText("2");
    ui->lineDummyCount->setValidator(new QIntValidator(0, 256, this));
    ui->lineDummyCount_2->setText("2");
    ui->lineDummyCount_2->setValidator(new QIntValidator(0, 256, this));

    ui->lineReadRepeatCount->setText("0");
    ui->lineReadRepeatCount->setValidator(new QIntValidator(0, 999999, this));
    ui->lineReadRepeatInterval->setText("500");
    ui->lineReadRepeatInterval->setValidator(new QIntValidator(0, 60000, this));

    ui->lineWriteRepeatCount->setText("0");
    ui->lineWriteRepeatCount->setValidator(new QIntValidator(0, 999999, this));
    ui->lineWriteRepeatInterval->setText("500");
    ui->lineWriteRepeatInterval->setValidator(new QIntValidator(0, 60000, this));

}

MainWindow::~MainWindow()
{
    if (deviceConnected)
        Usb2UisInterface::USBIO_CloseDevice(deviceIndex);

    delete ui;
}

void MainWindow::on_btnConnect_clicked()
{
    if (!deviceConnected) {
        deviceIndex = Usb2UisInterface::USBIO_OpenDevice();
        if (deviceIndex == 0xFF) {
            QMessageBox::warning(this, "錯誤", "無法連接USB裝置");
            return;
        }
        deviceConnected = true;
        ui->btnConnect->setText("Disconnect");
    } else {
        Usb2UisInterface::USBIO_CloseDevice(deviceIndex);
        deviceConnected = false;
        deviceIndex = 0xFF;
        ui->btnConnect->setText("Connect");
    }
}

void MainWindow::on_btnApplyConfig_clicked()
{
    if (!deviceConnected) return;

    int speedIndex = ui->comboSpiSpeed->currentIndex();     // bit3~0
    int modeIndex = ui->comboSpiMode->currentIndex();       // bit5~4
    BYTE configByte = (modeIndex << 4) | speedIndex;        // m/s bit7=0

    DWORD timeout = (ui->lineWriteTimeout->text().toUShort() << 16) |
            ui->lineReadTimeout->text().toUShort();

    if (!Usb2UisInterface::USBIO_SPISetConfig(deviceIndex, configByte, timeout)) {
        QMessageBox::warning(this, "錯誤", "SPI 設定失敗");
    } else {
        QMessageBox::information(this, "成功", "SPI 設定成功");
    }
}

void MainWindow::on_btnSpiRead_clicked()
{
    if (!deviceConnected) return;

    QByteArray cmd, recvBuffer;
    WORD readSize = ui->lineReadBytes->text().toUShort();

    if (!parseHexString(ui->lineReadCmd->text(), cmd) || cmd.size() != 4) {
        QMessageBox::warning(this, "錯誤", "請輸入有效4Bytes的HEX指令");
        return;
    }

    int delayMs = ui->lineSpiDelayMs->text().toInt();
    int dummyCount = ui->lineDummyCount->text().toInt();      // 預先 Dummy 0xFF 個數

    // 重複次數與間隔
    bool      repeatEnable   = ui->chkReadRepeatEnable->isChecked();
    int32_t   repeatCount    = ui->lineReadRepeatCount->text().toInt();
    int32_t   repeatInterval = ui->lineReadRepeatInterval->text().toInt();

    int32_t iteration = 0;

    while (true)
    {

        // Step 1: 傳送 Dummy 0xFF
        //------------------------------------------------------------------------------------
        // ✅ 拉 LOW: 啟動傳輸階段
        Usb2UisInterface::USBIO_SetCE(deviceIndex, false);

        if (dummyCount > 0) {
            QByteArray dummy(dummyCount, char(0xFF));
            if (!Usb2UisInterface::USBIO_SPIWrite(deviceIndex, nullptr, 0, (BYTE*)dummy.data(), dummy.size()))
            {
                QMessageBox::warning(this, "錯誤", "Dummy Bytes 傳送失敗");
                Usb2UisInterface::USBIO_SetCE(deviceIndex, true);
                return;
            }
        }

        delayBlockingUs(500);
        Usb2UisInterface::USBIO_SetCE(deviceIndex, true);
        delayBlockingMs(1);            //Refer AFE Spec.
        //+-----------------------------------------------------------------------------------


        // Step 2: 傳送命令後延遲
        //------------------------------------------------------------------------------------
        // ✅ 拉 LOW: 啟動傳輸階段
        Usb2UisInterface::USBIO_SetCE(deviceIndex, false);

        QByteArray dummyBuffer;
        Usb2UisInterface::USBIO_SPIWrite(deviceIndex,(BYTE*)cmd.data(), cmd.size(), nullptr, 0);

        // 延遲（保持 CS LOW）
        if (delayMs > 0)
        {
            // 阻塞延遲
            delayBlockingMs(delayMs);
        }

        recvBuffer.resize(readSize);
        if (!Usb2UisInterface::USBIO_SPIRead(deviceIndex,
                                             nullptr, 0, (BYTE*)recvBuffer.data(), readSize)) {
            QMessageBox::warning(this, "錯誤", "SPI讀取失敗");
            Usb2UisInterface::USBIO_SetCE(deviceIndex, true);  // 拉回 HIGH
            return;
        }
        //------------------------------------------------------------------------------------

        // ✅ 拉 HIGH: 結束傳輸階段
        Usb2UisInterface::USBIO_SetCE(deviceIndex, true);

        QString result;
        for (BYTE b : recvBuffer)
            result += QString("0x%1 ").arg(b, 2, 16, QChar('0')).toUpper();

        result.replace("X","x");

        QString timeStr = QTime::currentTime().toString("HH:mm:ss.zzz");
        ui->textSpiReadResult->appendPlainText(QString("[%1] Read : %2").arg(timeStr, result.trimmed()));

        iteration++;

        if (!repeatEnable || (repeatCount > 0 && iteration >= repeatCount)) break;

        QCoreApplication::processEvents(); // allow checkbox to be unchecked during loop
        if (!ui->chkReadRepeatEnable->isChecked()) break;
        QThread::msleep(repeatInterval);
    }

}


//SPI CMD+寫入
void MainWindow::on_btnSpiWrite_clicked()
{
    if (!deviceConnected) return;

    QByteArray cmd, data;
    if (!parseHexString(ui->lineWriteCmd->text(), cmd) || cmd.size() != 4) {
        QMessageBox::warning(this, "錯誤", "請輸入有效4Bytes的HEX指令");
        return;
    }

    if (!parseHexString(ui->textWriteData->toPlainText(), data)) {
        QMessageBox::warning(this, "錯誤", "請輸入正確 HEX 格式資料");
        return;
    }

    int delayMs = ui->lineSpiDelayMs->text().toInt();
    int dummyCount = ui->lineDummyCount_2->text().toInt();      // 預先 Dummy 0xFF 個數

    // 重複次數與間隔
    bool     repeatEnable   = ui->chkWriteRepeatEnable->isChecked();
    int32_t  repeatCount    = ui->lineWriteRepeatCount->text().toInt();
    int32_t  repeatInterval = ui->lineWriteRepeatInterval->text().toInt();

    int32_t iteration = 0;
    while (true) {

        // Step 1: 傳送 Dummy 0xFF
        //------------------------------------------------------------------------------------
        // ✅ 拉 LOW: 啟動傳輸階段
        Usb2UisInterface::USBIO_SetCE(deviceIndex, false);

        if (dummyCount > 0) {
            QByteArray dummy(dummyCount, char(0xFF));
            if (!Usb2UisInterface::USBIO_SPIWrite(deviceIndex, nullptr, 0, (BYTE*)dummy.data(), dummy.size()))
            {
                QMessageBox::warning(this, "錯誤", "Dummy Bytes 傳送失敗");
                Usb2UisInterface::USBIO_SetCE(deviceIndex, true);
                return;
            }
        }

        delayBlockingUs(500);
        Usb2UisInterface::USBIO_SetCE(deviceIndex, true);
        delayBlockingMs(1);            //Refer AFE Spec.
        //+-----------------------------------------------------------------------------------

        // Step 2: 傳送命令後延遲
        //------------------------------------------------------------------------------------
        // ✅ 拉 LOW: 啟動傳輸階段
        Usb2UisInterface::USBIO_SetCE(deviceIndex, false);

        Usb2UisInterface::USBIO_SPIWrite(deviceIndex,
                                         (BYTE*)cmd.data(), cmd.size(), nullptr, 0);


        Usb2UisInterface::USBIO_SetCE(deviceIndex, false);

        // 延遲（保持 CS LOW）
        if (delayMs > 0)
        {
            // 阻塞延遲
            delayBlockingMs(delayMs);
        }

        delayBlockingUs(500); //必須先阻塞時間， 傳送時序問題延遲500us以上
        if (!Usb2UisInterface::USBIO_SPIWrite(deviceIndex,
                                              nullptr, 0, (BYTE*)data.data(), data.size()))
        {
            QMessageBox::warning(this, "錯誤", "SPI資料寫入失敗");
            Usb2UisInterface::USBIO_SetCE(deviceIndex, true);  // 拉回 HIGH
            return;
        }
        delayBlockingUs(500); //必須先阻塞時間， 傳送時序問題延遲500us以上

        // ✅ 拉 HIGH: 結束傳輸階段
        Usb2UisInterface::USBIO_SetCE(deviceIndex, true);
        //------------------------------------------------------------------------------------


        // 顯示寫入資料 HEX 字串到 textSpiReadResult
        QString result;
        for (BYTE b : data)
            result += QString("0x%1 ").arg(b, 2, 16, QChar('0')).toUpper();

        result.replace("X","x");

        QString timeStr = QTime::currentTime().toString("HH:mm:ss.zzz");
        ui->textSpiReadResult->appendPlainText(QString("[%1] Wrote: %2").arg(timeStr, result.trimmed()));

        iteration++;

        if (!repeatEnable || (repeatCount > 0 && iteration >= repeatCount)) break;

        QCoreApplication::processEvents();
        if (!ui->chkWriteRepeatEnable->isChecked()) break;
        QThread::msleep(repeatInterval);
    }
}

void MainWindow::on_btnClearResult_clicked()
{
    ui->textSpiReadResult->clear();
}

bool MainWindow::parseHexString(const QString& input, QByteArray& output)
{
    output.clear();
    QStringList tokens = input.trimmed().split(QRegExp("\\s+"), QString::SkipEmptyParts);
    for (const QString &token : tokens)
    {
        bool ok;
        QString hex = token;
        if (hex.startsWith("0x", Qt::CaseInsensitive))
            hex = hex.mid(2);
        if (hex.length() > 2)
            return false;
        BYTE byte = hex.toUShort(&ok, 16);
        if (!ok) return false;
        output.append(byte);
    }
    return true;
}

void MainWindow::delayBlockingMs(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}



// 阻塞延遲（以微秒為單位）
void MainWindow::delayBlockingUs(int usec)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.nsecsElapsed() < usec * 1000) {
        // busy wait
    }
}

