#include "zmainwindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QTextCursor>
#include <QScrollBar>
#include <QDebug>

ZMainWindow::ZMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_workerThread=nullptr;
    m_uartWorker=nullptr;
    m_uartParser=nullptr;
    m_ringBuffer=nullptr;

    //create shared ring buffer.
    m_ringBuffer=new ZRingBuffer(4096);

    //create threads.
    m_workerThread=new QThread(this);
    m_uartWorker=new ZUartWorker(m_ringBuffer);
    m_uartWorker->moveToThread(m_workerThread);
    connect(m_workerThread,&QThread::finished,m_uartWorker,&ZUartWorker::deleteLater);
    connect(m_uartWorker,&ZUartWorker::errorMessage,this,&ZMainWindow::onMessage);
    connect(m_uartWorker,&ZUartWorker::statusMessage,this,&ZMainWindow::onMessage);

    //parse thread, runs in main thread for easy UI updates.
    m_uartParser=new ZUartParser(m_ringBuffer,this);
    connect(m_uartParser,&ZUartParser::spectrumRange,this,&ZMainWindow::onSpectrumRange);
    connect(m_uartParser,&ZUartParser::frameReceived,this,&ZMainWindow::onNewFrame);
    connect(m_uartParser,&ZUartParser::errorMessage,this,&ZMainWindow::onMessage);
    connect(m_uartParser,&ZUartParser::statusMessage,this,&ZMainWindow::onMessage);
    connect(m_uartParser,&ZUartParser::newImage,this,&ZMainWindow::onNewImage);

    //start UART thread.
    m_workerThread->start();

    QWidget *centralWidget = new QWidget(this);
    this->setCentralWidget(centralWidget);
    this->m_vLayoutMain = new QVBoxLayout(centralWidget);

    m_ll=new QLabel;
    m_ll->setMinimumSize(600,300);
    //scale QImage in working thread for high efficiency.
    m_ll->setScaledContents(true);

    m_teLog=new QTextEdit;
    m_tbGetRange=new QToolButton;
    m_tbGetRange->setText("Support Range");
    m_tbGetSpectrum=new QToolButton;
    m_tbGetSpectrum->setText(tr("Read Spectrum"));

    m_tbInitDev=new QToolButton;
    m_tbInitDev->setText("Init Dev");

    m_cbVerbose=new QCheckBox(tr("Verbose"));

    m_hLayoutBtn=new QHBoxLayout;
    m_hLayoutBtn->addWidget(m_tbInitDev);
    m_hLayoutBtn->addWidget(m_tbGetRange);
    m_hLayoutBtn->addWidget(m_tbGetSpectrum);
    m_hLayoutBtn->addWidget(m_cbVerbose);
    m_hLayoutBtn->addStretch(1);

    m_vLayoutMain->addWidget(m_ll);
    m_vLayoutMain->addLayout(m_hLayoutBtn);
    m_vLayoutMain->addWidget(m_teLog);

    connect(m_tbInitDev,&QToolButton::clicked,this,&ZMainWindow::onConnectClicked);
    connect(m_tbGetRange,&QToolButton::clicked,this,&ZMainWindow::onSendClicked);
    connect(m_tbGetSpectrum,&QToolButton::clicked,this,&ZMainWindow::onGetSpectrum);
    connect(m_cbVerbose,&QCheckBox::checkStateChanged, m_uartParser,&ZUartParser::verboseMode);
}

ZMainWindow::~ZMainWindow()
{
    if(this->m_workerThread)
    {
        if(this->m_workerThread->isRunning())
        {
            this->m_workerThread->quit();
            this->m_workerThread->wait();
        }
        delete this->m_workerThread;
        this->m_workerThread=nullptr;
    }
    if(m_teLog) {
        delete m_teLog;
    }
}

void ZMainWindow::onConnectClicked()
{
    QMetaObject::invokeMethod(m_uartWorker,"initPort",
                              Qt::QueuedConnection,
                              Q_ARG(QString,"ttyUSB2"),
                              Q_ARG(qint32, 256000));
    // if (m_workerThread && m_workerThread->isRunning()) {
    //     if (m_worker) {
    //         QMetaObject::invokeMethod(m_worker, "closePort", Qt::QueuedConnection);
    //     }
    //     return;
    // }

    // // Initialize Thread and Worker
    // m_workerThread = new QThread(this);
    // m_worker = new ZSerialWorker();

    // // Move worker to thread
    // m_worker->moveToThread(m_workerThread);

    // // Connect Worker signals to Main Window slots (Cross-thread communication)
    // connect(m_workerThread,&QThread::started,m_worker,&ZSerialWorker::ZSlotBegin);
    // connect(m_workerThread, &QThread::started, m_worker, [this]() {
    //     QMetaObject::invokeMethod(m_worker, "initPort", Qt::QueuedConnection,
    //                               Q_ARG(QString, "ttyUSB2"),
    //                               Q_ARG(qint32, 256000),
    //                               Q_ARG(QSerialPort::DataBits, QSerialPort::Data8),
    //                               Q_ARG(QSerialPort::Parity, QSerialPort::NoParity),
    //                               Q_ARG(QSerialPort::StopBits, QSerialPort::OneStop));
    // });

    // connect(m_worker, &ZSerialWorker::dataReceived, this, &ZMainWindow::onUpdateUI);
    // connect(m_worker, &ZSerialWorker::errorOccurred, this, &ZMainWindow::onShowError);
    // connect(m_worker, &ZSerialWorker::portStatusChanged, this, &ZMainWindow::onPortStatusChanged);

    // // Cleanup when thread finishes
    // connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    // connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);

    // m_workerThread->start();
}
void ZMainWindow::onSendClicked()
{
    unsigned char cmd[]={0xCC,0x01,0x09,0x00,0x00,0x0F,0xE5,0x0D,0x0A};
    QByteArray data=QByteArray::fromRawData(reinterpret_cast<const char*>(cmd),sizeof(cmd));

    //Method 1: Using QMetaObject::invokeMethod (Direct Call)
    //This is the most direct way if you just want to trigger the function without setting up extra signals/slots.

    //Qt::QueuedConnection‌: This is crucial. It posts an event to the worker thread's event loop. The function will execute asynchronously in the worker thread.
    //Q_ARG‌: Used to pass arguments. Ensure the type matches the function signature exactly (QByteArray).
    QMetaObject::invokeMethod(m_uartWorker,"sendData",Qt::QueuedConnection,Q_ARG(QByteArray,data));
}
void ZMainWindow::onGetSpectrum()
{
    unsigned char cmd[]={0xCC,0x01,0x09,0x00,0x00,0x02,0xD8,0x0D,0x0A};
    QByteArray data=QByteArray::fromRawData(reinterpret_cast<const char*>(cmd),sizeof(cmd));

    //Method 1: Using QMetaObject::invokeMethod (Direct Call)
    //This is the most direct way if you just want to trigger the function without setting up extra signals/slots.

    //Qt::QueuedConnection‌: This is crucial. It posts an event to the worker thread's event loop. The function will execute asynchronously in the worker thread.
    //Q_ARG‌: Used to pass arguments. Ensure the type matches the function signature exactly (QByteArray).
    QMetaObject::invokeMethod(m_uartWorker,"sendData",Qt::QueuedConnection,Q_ARG(QByteArray,data));
}
void ZMainWindow::onUpdateUI(const QByteArray &data)
{
    QString timestamp = QDateTime::currentDateTime().toString("[hh:mm:ss.zzz] ");
    QString hexStr;
    // Optional: Convert to Hex for display if needed
    // for (char c : data) hexStr += QString("%1 ").arg(static_cast<unsigned char>(c), 2, 16, QChar('0'));

    QString displayText = timestamp + QString::fromUtf8(data);
    //    m_receiveEdit->append(displayText);
    qDebug()<<displayText;
    for(int i=0;i<data.length();i++)
    {
        qDebug("%02x ",(unsigned char)data[i]);
    }
}
void ZMainWindow::onSpectrumRange(const QString &start, const QString &end)
{
    onMessage("Supported spectrum range: "+start+"nm - "+end+"nm.");
}
void ZMainWindow::onMessage(const QString &message)
{
    QTextCursor cursor=m_teLog->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.insertText(message+"\n");
    m_teLog->verticalScrollBar()->setValue(0);
}
void ZMainWindow::onNewImage(const QImage &image)
{
    QPixmap pixmap=QPixmap::fromImage(image);
    m_ll->setPixmap(pixmap);
}
void ZMainWindow::onNewFrame(const QByteArray &payload)
{
    qDebug()<<"Valid New Frame:\n";
    qDebug()<<payload.toHex()<<"\n";

}
void ZMainWindow::onPortStatusChanged(bool isOpen)
{
    if (isOpen) {
        qDebug()<<"Status: Open";
    } else {
        qDebug()<<"Status: Closed";

        if (m_workerThread) {
            m_workerThread->quit();
            m_workerThread->wait();
            m_workerThread = nullptr;
        }
    }
}
