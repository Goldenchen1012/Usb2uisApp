#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "usb2uis_interface.h"

#include <QMessageBox>
#include <QThread>
#include <QEventLoop>
#include <QTimer>
#include <QTime>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>


#define USB2UIS_APP_NAME_STR         QString("Usb2uisApp")
#define USB2UIS_APP_VERSION_STR      QString("V1.2")

#define USB2UIS_GPIO_IO1_MASKBIT         (~0x01)
#define USB2UIS_GPIO_IO2_MASKBIT         (~0x02)
#define USB2UIS_GPIO_IO3_MASKBIT         (~0x04)
#define USB2UIS_GPIO_IO4_MASKBIT         (~0x08)
#define USB2UIS_GPIO_IO5_MASKBIT         (~0x10)
#define USB2UIS_GPIO_IO6_MASKBIT         (~0x20)
#define USB2UIS_GPIO_IO7_MASKBIT         (~0x40)
#define USB2UIS_GPIO_IO8_MASKBIT         (~0x80)

BYTE deviceIndex = 0;

/* 讀檔並回傳 <顯示文字, HEX字串> 清單，只取第一欄=1 的行 */
static QList<QPair<QString, QString>> loadCmdFile(const QString &fileName)
{
    QList<QPair<QString, QString>> list;

    // 使用程式執行檔所在目錄為 base path
    QString fullPath = QCoreApplication::applicationDirPath() + QDir::separator() + fileName;

    qDebug() << "[DEBUG] Try to load file:" << fullPath;

    QFile f(fullPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[ERROR] Cannot open file:" << fullPath;
        return list;
    }

    QTextStream ts(&f);
    const QRegularExpression re("^\\s*(\\d)\\s*,\\s*\"([^\"]+)\"\\s*,\\s*(.+)$");

    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        auto m = re.match(line);
        if (!m.hasMatch()) continue;
        if (m.captured(1) != "1") continue;

        const QString label = m.captured(2);
        const QString hex = m.captured(3).trimmed();

        list.append({label, hex});
    }

    return list;
}

void MainWindow::loadReadCmdSet()
{
    mapSetDescription.clear();
    ui->comboReadCmdSet->clear();

    QFile f(QCoreApplication::applicationDirPath() + "/SPI_READ_CMD_SET.txt");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream ts(&f);
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        QRegularExpression re("^\\s*(\\d)\\s*,\\s*(\\d+)\\s*,\\s*\"([^\"]+)\"\\s*$");
        auto m = re.match(line);
        if (m.hasMatch() && m.captured(1) == "1") {
            int setId = m.captured(2).toInt();
            QString desc = m.captured(3);
            mapSetDescription[setId] = desc;
            ui->comboReadCmdSet->addItem(desc, setId);
        }
    }
}

void MainWindow::loadReadCmdList()
{
    listSetCmdsOrdered.clear();

    QFile f(QCoreApplication::applicationDirPath() + "/SPI_READ_CMD_LIST.txt");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream ts(&f);
    while (!ts.atEnd()) {
        const QString line = ts.readLine();
        QRegularExpression re("^\\s*(\\d)\\s*,\\s*(\\d+)\\s*,\\s*\"[^\"]*\"\\s*,\\s*(.+)$");
        auto m = re.match(line);
        if (m.hasMatch() && m.captured(1) == "1") {
            int setId = m.captured(2).toInt();
            QString hex = m.captured(3).trimmed();
            listSetCmdsOrdered.append(qMakePair(setId, hex));
        }
    }
}

void MainWindow::GpioSet(eTypeGPIO_IO_PORT eGpio)
{
    BYTE value;         // 1=High, 0 = Low
    BYTE mask;

    if (!deviceConnected) return;

    value = 0;
    value |=(1<<eGpio);
    mask = (~value);

    Usb2UisInterface::USBIO_GPIOWrite(deviceIndex, value, mask);
}

void MainWindow::SpiDirectionHighLow(bool bDirNorth, bool bHigh){
    if(bDirNorth == true)
    {
        GpioSet(USB2UIS_GPIO_IO1);
        Usb2UisInterface::USBIO_SetCE(deviceIndex, bHigh);
    }
    else
    {
        Usb2UisInterface::USBIO_SetCE(deviceIndex, true);
        if(bHigh == true)
        {
            GpioSet(USB2UIS_GPIO_IO1);
        }
        else
        {
            GpioClear(USB2UIS_GPIO_IO1);
        }
    }
}

void MainWindow::GpioClear(eTypeGPIO_IO_PORT eGpio)
{
    BYTE value;         // 1=High, 0 = Low
    BYTE mask;

    if (!deviceConnected) return;

    value = 0;
    value |=(1<<eGpio);
    mask = (~value);

    Usb2UisInterface::USBIO_GPIOWrite(deviceIndex, ~value, mask);
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle(USB2UIS_APP_NAME_STR + " " + USB2UIS_APP_VERSION_STR);

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

    // Connect 按鈕點擊事件 → 對應的槽函數
    connect(ui->btnLoadReadCmdList,    &QPushButton::clicked, this, &MainWindow::on_btnLoadReadCmdList_clicked);
    connect(ui->btnLoadWriteCmdList,   &QPushButton::clicked, this, &MainWindow::on_btnLoadWriteCmdList_clicked);
    connect(ui->btnLoadWriteDataList,  &QPushButton::clicked, this, &MainWindow::on_btnLoadWriteDataList_clicked);

    connect(ui->comboReadCmdList,   &QComboBox::currentTextChanged,
            this, &MainWindow::onReadCmdChosen);
    connect(ui->comboWriteCmdList,  &QComboBox::currentTextChanged,
            this, &MainWindow::onWriteCmdChosen);
    connect(ui->comboWriteDataList, &QComboBox::currentTextChanged,
            this, &MainWindow::onWriteDataChosen);

    connect(ui->btnLoadReadCmdSet, &QPushButton::clicked, this, &MainWindow::on_btnLoadReadCmdSet_clicked);
    connect(ui->comboReadCmdSet, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::on_comboReadCmdSet_currentIndexChanged);

    //預設為北向
    ui->rdoNorth->setChecked(true);
    bDirNorth = true;
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


    BYTE dir = 0b00000000;     // IO1(PIN J7-10)   bit1/0 = 0(output), 其餘1(input)
    if(!Usb2UisInterface::USBIO_SetGPIOConfig(deviceIndex, dir)){
        QMessageBox::warning(this, "GPIO", "GPIO 設定Fail");
    }

    //Gpio set High
    GpioSet(USB2UIS_GPIO_IO1);
    GpioSet(USB2UIS_GPIO_IO2);

    if (!Usb2UisInterface::USBIO_SPISetConfig(deviceIndex, configByte, timeout)) {
        QMessageBox::warning(this, "錯誤", "Device 設定失敗");
    } else {
        QMessageBox::information(this, "成功", "Device 設定成功");
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
        SpiDirectionHighLow(bDirNorth, false); //Low

        if (dummyCount > 0) {
            QByteArray dummy(dummyCount, char(0xFF));
            if (!Usb2UisInterface::USBIO_SPIWrite(deviceIndex, nullptr, 0, (BYTE*)dummy.data(), dummy.size()))
            {
                QMessageBox::warning(this, "錯誤", "Dummy Bytes 傳送失敗");
                SpiDirectionHighLow(bDirNorth, true); //High
                return;
            }
        }

        delayBlockingUs(500);
        SpiDirectionHighLow(bDirNorth, true); //High
        delayBlockingMs(1);            //Refer AFE Spec.
        //+-----------------------------------------------------------------------------------


        // Step 2: 傳送命令後延遲
        //------------------------------------------------------------------------------------
        // ✅ 拉 LOW: 啟動傳輸階段
        SpiDirectionHighLow(bDirNorth, false); //Low

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
            SpiDirectionHighLow(bDirNorth, true); //High
            return;
        }
        //------------------------------------------------------------------------------------

        // ✅ 拉 HIGH: 結束傳輸階段
        SpiDirectionHighLow(bDirNorth, true); //High

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


void MainWindow::on_btnSpiRead2_clicked()
{
    if (!deviceConnected) return;

    int setIndex = ui->comboReadCmdSet->currentIndex();
    if (setIndex < 0) {
        QMessageBox::warning(this, "錯誤", "請先選擇 SPI READ CMD SET");
        return;
    }

    int setId = ui->comboReadCmdSet->itemData(setIndex).toInt();
    QStringList cmdList;

    for (const auto& pair : listSetCmdsOrdered) {
        if (pair.first == setId)
            cmdList << pair.second;
    }

    if (cmdList.isEmpty()) return;

    int repeatCount = ui->lineReadRepeatCount->text().toInt();
    bool repeatEnable = ui->chkReadRepeatEnable->isChecked();
    int repeatInterval = ui->lineReadRepeatInterval->text().toInt();

    int delayMs = ui->lineSpiDelayMs->text().toInt();
    int dummyCount = ui->lineDummyCount->text().toInt();
    int readSize = ui->lineReadBytes->text().toInt();

    int iteration = 0;
    while (true) {
        for (int i = 0; i < cmdList.size(); ++i) {
            QString hex = cmdList[i];
            QByteArray cmd;
            if (!parseHexString(hex, cmd)) continue;

            // ListView 指示目前執行第幾條
            if (cmdListModel) {
                QModelIndex index = cmdListModel->index(i);
                ui->listViewReadCmds->setCurrentIndex(index);
            }

            // Dummy
            SpiDirectionHighLow(bDirNorth, false); //Low
            if (dummyCount > 0) {
                QByteArray dummy(dummyCount, char(0xFF));
                Usb2UisInterface::USBIO_SPIWrite(deviceIndex, nullptr, 0, (BYTE*)dummy.data(), dummyCount);
            }

            delayBlockingUs(500);
            SpiDirectionHighLow(bDirNorth, true); //High
            delayBlockingUs(500);

            // 指令傳送
            SpiDirectionHighLow(bDirNorth, false); //Low
            Usb2UisInterface::USBIO_SPIWrite(deviceIndex, (BYTE*)cmd.data(), cmd.size(), nullptr, 0);
            if (delayMs > 0) delayBlockingMs(delayMs);

            QByteArray recv;
            recv.resize(readSize);
            Usb2UisInterface::USBIO_SPIRead(deviceIndex, nullptr, 0, (BYTE*)recv.data(), readSize);
            SpiDirectionHighLow(bDirNorth, true); //High

            QString result;
            for (BYTE b : recv)
                result += QString("0x%1 ").arg(b, 2, 16, QChar('0')).toUpper();

            result.replace("X","x");

            ui->textSpiReadResult->appendPlainText(QString("[%1] Read : %2")
                                                   .arg(QTime::currentTime().toString("HH:mm:ss.zzz")).arg(result.trimmed()));


            //QCoreApplication::processEvents();
            //QThread::msleep(1);
        }

        ++iteration;
        if (!repeatEnable || (repeatCount > 0 && iteration >= repeatCount)) break;
        QCoreApplication::processEvents();
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
        SpiDirectionHighLow(bDirNorth, false); //Low

        if (dummyCount > 0) {
            QByteArray dummy(dummyCount, char(0xFF));
            if (!Usb2UisInterface::USBIO_SPIWrite(deviceIndex, nullptr, 0, (BYTE*)dummy.data(), dummy.size()))
            {
                QMessageBox::warning(this, "錯誤", "Dummy Bytes 傳送失敗");
                SpiDirectionHighLow(bDirNorth, true); //High
                return;
            }
        }

        delayBlockingUs(500);
        SpiDirectionHighLow(bDirNorth, true); //High
        delayBlockingUs(500);                //Refer AFE Spec.
        //+-----------------------------------------------------------------------------------

        // Step 2: 傳送命令後延遲
        //------------------------------------------------------------------------------------
        // ✅ 拉 LOW: 啟動傳輸階段
        SpiDirectionHighLow(bDirNorth, false); //Low
        delayBlockingUs(500);

        Usb2UisInterface::USBIO_SPIWrite(deviceIndex,
                                         (BYTE*)cmd.data(), cmd.size(), nullptr, 0);


        delayBlockingUs(500);

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
            SpiDirectionHighLow(bDirNorth, true); //High
            return;
        }

        delayBlockingMs(2); //注意~~~MUST 必須先阻塞時間， 傳送時序問題延遲500us以上(寫入資料太多時，請加長時間)

        // ✅ 拉 HIGH: 結束傳輸階段
        SpiDirectionHighLow(bDirNorth, true); //High
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

/* --------- ① 讀取 READ-CMD 清單按鈕 ---------- */
void MainWindow::on_btnLoadReadCmdList_clicked()
{
    ui->comboReadCmdList->clear();
    ui->comboReadCmdList->setProperty("hexList", QVariant());   // 清旗標

    auto list = loadCmdFile("SPI_READ_ONE_CMD_LIST.txt");
    QVariantList hexList;
    for (const auto &p : list) {
        ui->comboReadCmdList->addItem(p.first);
        hexList << p.second;
    }
    ui->comboReadCmdList->setProperty("hexList", hexList);
    ui->comboReadCmdList->setCurrentIndex(-1);

    if (!list.isEmpty()) {
        ui->comboReadCmdList->setCurrentIndex(0);
        ui->lineReadCmd->setText(list[0].second);
    }
}

/* Combo 被選中 → 將 HEX 填入 lineReadCmd */
void MainWindow::onReadCmdChosen(const QString &text)
{
    auto hexList = ui->comboReadCmdList->property("hexList").toList();
    int idx = ui->comboReadCmdList->currentIndex();
    if (idx >= 0 && idx < hexList.size()) {
        ui->lineReadCmd->clear();
        ui->lineReadCmd->setText(hexList[idx].toString());
    }
}

/* --------- ② 讀取 WRITE-CMD 清單按鈕 ---------- */
void MainWindow::on_btnLoadWriteCmdList_clicked()
{
    ui->comboWriteCmdList->clear();
    QVariantList hexList;

    auto list = loadCmdFile("SPI_WRITE_CMD_LIST.txt");
    for (const auto &p : list) {
        ui->comboWriteCmdList->addItem(p.first);
        hexList << p.second;
    }
    ui->comboWriteCmdList->setProperty("hexList", hexList);
    ui->comboWriteCmdList->setCurrentIndex(-1);

    if (!list.isEmpty()) {
        ui->comboWriteCmdList->setCurrentIndex(0);
        ui->lineWriteCmd->setText(list[0].second);
    }
}

/* Combo 被選中 → 將 HEX 填入 lineWriteCmd */
void MainWindow::onWriteCmdChosen(const QString &)
{
    auto hexList = ui->comboWriteCmdList->property("hexList").toList();
    int idx = ui->comboWriteCmdList->currentIndex();
    if (idx >= 0 && idx < hexList.size()) {
        ui->lineWriteCmd->clear();
        ui->lineWriteCmd->setText(hexList[idx].toString());
    }

}

/* --------- ③ 讀取 WRITE-DATA 清單按鈕 ---------- */
void MainWindow::on_btnLoadWriteDataList_clicked()
{
    ui->comboWriteDataList->clear();
    QVariantList hexList;

    auto list = loadCmdFile("SPI_WRITE_DATA_LIST.txt");
    for (const auto &p : list) {
        ui->comboWriteDataList->addItem(p.first);
        hexList << p.second;
    }
    ui->comboWriteDataList->setProperty("hexList", hexList);
    ui->comboWriteDataList->setCurrentIndex(-1);


    if (!list.isEmpty()) {
        ui->comboWriteDataList->setCurrentIndex(0);
        ui->textWriteData->setPlainText(list[0].second);
    }
}

/* Combo 被選中 → 將 HEX 填入 textWriteData (多行元件) */
void MainWindow::onWriteDataChosen(const QString &)
{
    auto hexList = ui->comboWriteDataList->property("hexList").toList();
    int idx = ui->comboWriteDataList->currentIndex();
    if (idx >= 0 && idx < hexList.size()) {
        ui->textWriteData->clear();
        ui->textWriteData->setPlainText(hexList[idx].toString());
    }
}

void MainWindow::on_btnLoadReadCmdSet_clicked()
{
    loadReadCmdSet();
    loadReadCmdList();

    // ⚠️ 若成功載入至少一筆 SET，就選第一筆（index = 0）
    if (ui->comboReadCmdSet->count() > 0) {
        ui->comboReadCmdSet->setCurrentIndex(0);     // 會自動觸發 on_comboReadCmdSet_currentIndexChanged
    } else {
        ui->comboReadCmdSet->setCurrentIndex(-1);    // 沒資料就清空
        ui->listViewReadCmds->setModel(nullptr);
    }
}

void MainWindow::on_comboReadCmdSet_currentIndexChanged(int index)
{
    if (index < 0) return;

    int setId = ui->comboReadCmdSet->itemData(index).toInt();
    QStringList cmdList;

    for (const auto& pair : listSetCmdsOrdered) {
        if (pair.first == setId) {
            cmdList << pair.second;
        }
    }

    if (cmdListModel) delete cmdListModel;
    cmdListModel = new QStringListModel(cmdList, this);
    ui->listViewReadCmds->setModel(cmdListModel);
}

void MainWindow::on_rdoNorth_clicked()
{
    bDirNorth = true;
}


void MainWindow::on_rdoSouth_clicked()
{
    bDirNorth = false;
}

