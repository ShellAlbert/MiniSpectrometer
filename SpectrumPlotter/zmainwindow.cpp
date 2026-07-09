#include "zmainwindow.h"
#include <QMessageBox>
#include <QDateTime>
#include <QPainter>
#include <QTextCursor>
#include <QScrollBar>
#include <QDebug>
#include <QScrollArea>

ZMainWindow::ZMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_timer=nullptr;

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
    connect(m_uartParser,&ZUartParser::errorMessage,this,&ZMainWindow::onMessage);
    connect(m_uartParser,&ZUartParser::statusMessage,this,&ZMainWindow::onMessage);
    connect(m_uartParser,&ZUartParser::newImage,this,&ZMainWindow::onNewImage);

    //start UART thread.
    m_workerThread->start();


    //Add QLabel to QScrollArea,
    //so we can scroll if the image is too big.
    m_ll=new QLabel;
    m_ll->setMinimumSize(600,300);
    //scale QImage in working thread for high efficiency.
    //m_ll->setScaledContents(true);


    m_vlayoutScrollArea=new QVBoxLayout;
    m_vlayoutScrollArea->addWidget(m_ll);
    QWidget *m_scrollAreaWidget=new QWidget(this);
    m_scrollAreaWidget->setLayout(m_vlayoutScrollArea);
    m_scrollArea=new QScrollArea;
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_scrollArea->setWidget(m_scrollAreaWidget);


    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    m_vLayoutMain = new QVBoxLayout(centralWidget);

    m_teLog=new QTextEdit;
    m_teLog->setMinimumHeight(100);
    m_tbGetRange=new QToolButton;
    m_tbGetRange->setText("Support Range");
    m_tbGetSpectrum=new QToolButton;
    m_tbGetSpectrum->setText(tr("Read Spectrum"));

    m_tbInitDev=new QToolButton;
    m_tbInitDev->setText("Init Dev");

    m_cbVerbose=new QCheckBox(tr("Verbose"));
    m_cbPeriodically=new QCheckBox(tr("Periodically"));
    m_llCounts=new QLabel;
    m_llCounts->setText("0");

    m_tbZoomIn=new QToolButton;
    m_tbZoomIn->setText("Zoom In");

    m_tbZoomOut=new QToolButton;
    m_tbZoomOut->setText("Zoom Out");

    m_hLayoutBtn=new QHBoxLayout;
    m_hLayoutBtn->addWidget(m_tbInitDev);
    m_hLayoutBtn->addWidget(m_tbGetRange);
    m_hLayoutBtn->addWidget(m_tbGetSpectrum);
    m_hLayoutBtn->addWidget(m_cbVerbose);
    m_hLayoutBtn->addWidget(m_cbPeriodically);
    m_hLayoutBtn->addWidget(m_llCounts);
    m_hLayoutBtn->addStretch(1);
    m_hLayoutBtn->addWidget(m_tbZoomIn);
    m_hLayoutBtn->addWidget(m_tbZoomOut);

    m_spliter=new QSplitter(Qt::Vertical);
    m_spliter->addWidget(m_scrollArea);
    m_spliter->addWidget(m_teLog);
    // m_spliter->setStretchFactor(0,4);
    // m_spliter->setStretchFactor(1,1);

    m_vLayoutMain->addWidget(m_spliter);
    m_vLayoutMain->addLayout(m_hLayoutBtn);
    connect(m_tbInitDev,&QToolButton::clicked,this,&ZMainWindow::onConnectClicked);
    connect(m_tbGetRange,&QToolButton::clicked,this,&ZMainWindow::onSendClicked);
    connect(m_tbGetSpectrum,&QToolButton::clicked,this,&ZMainWindow::onGetSpectrum);
    connect(m_cbVerbose,&QCheckBox::checkStateChanged, m_uartParser,&ZUartParser::verboseMode);
    connect(m_cbVerbose,&QCheckBox::checkStateChanged, m_uartWorker,&ZUartWorker::verboseMode);
    connect(m_cbPeriodically,&QCheckBox::checkStateChanged,this,&ZMainWindow::onPeriodically);

    connect(m_tbZoomIn,&QToolButton::clicked,m_uartParser,&ZUartParser::onZoomIn);
    connect(m_tbZoomOut,&QToolButton::clicked,m_uartParser,&ZUartParser::onZoomOut);

    connect(this,&ZMainWindow::sendCommand,m_uartWorker,&ZUartWorker::sendData);
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

void ZMainWindow::onPeriodically(Qt::CheckState state)
{
    if(state==Qt::Checked)
    {
        if(m_timer==nullptr)
        {
            m_timer=new QTimer(this);
            connect(m_timer,&QTimer::timeout,this,&ZMainWindow::onTimeout);
            onMessage(tr("Read spectrum periodically started, 50ms."));
            m_cntPeriodically=0;
            m_timer->start(50);
        }
    }else{
        m_timer->stop();
        delete this->m_timer;
        this->m_timer=nullptr;
        onMessage(tr("Read spectrum periodically stopped."));
    }
}
void ZMainWindow::onTimeout()
{
    onGetSpectrum();
    m_cntPeriodically++;
    m_llCounts->setText(QString::number(m_cntPeriodically,10));
}

void ZMainWindow::onConnectClicked()
{
    QMetaObject::invokeMethod(m_uartWorker,"initPort",
                              Qt::QueuedConnection,
                              Q_ARG(QString,"ttyUSB0"),
                              Q_ARG(qint32, 256000));
}
void ZMainWindow::onSendClicked()
{
    unsigned char cmd[]={0xCC,0x01,0x09,0x00,0x00,0x0F,0xE5,0x0D,0x0A};
    //QByteArray data=QByteArray::fromRawData(reinterpret_cast<const char*>(cmd),sizeof(cmd));
    QByteArray data(reinterpret_cast<const char*>(cmd),sizeof(cmd));

    //Method 1: Using QMetaObject::invokeMethod (Direct Call)
    //This is the most direct way if you just want to trigger the function without setting up extra signals/slots.

    //Qt::QueuedConnection‌: This is crucial. It posts an event to the worker thread's event loop. The function will execute asynchronously in the worker thread.
    //Q_ARG‌: Used to pass arguments. Ensure the type matches the function signature exactly (QByteArray).
    //QMetaObject::invokeMethod(m_uartWorker,"sendData",Qt::QueuedConnection,Q_ARG(QByteArray,data));
    emit sendCommand(data);
}
void ZMainWindow::onGetSpectrum()
{
    unsigned char cmd[]={0xCC,0x01,0x09,0x00,0x00,0x02,0xD8,0x0D,0x0A};
    //QByteArray data=QByteArray::fromRawData(reinterpret_cast<const char*>(cmd),sizeof(cmd));
    QByteArray data(reinterpret_cast<const char*>(cmd),sizeof(cmd));

    //Method 1: Using QMetaObject::invokeMethod (Direct Call)
    //This is the most direct way if you just want to trigger the function without setting up extra signals/slots.

    //Qt::QueuedConnection‌: This is crucial. It posts an event to the worker thread's event loop. The function will execute asynchronously in the worker thread.
    //Q_ARG‌: Used to pass arguments. Ensure the type matches the function signature exactly (QByteArray).
    //QMetaObject::invokeMethod(m_uartWorker,"sendData",Qt::QueuedConnection,Q_ARG(QByteArray,data));
    emit sendCommand(data);
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
void ZMainWindow::onNewImage(const QImage &backgroundImg, const QImage &foregroundImg, const QRect &validRect)
{
    QImage img(backgroundImg.size(),QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing);

    //image will be scaled while painting.
    QRect targetRect(0,0,m_ll->width(),m_ll->height());

    painter.drawImage(0,0,backgroundImg);
    painter.drawImage(0,0,foregroundImg);
    painter.end();
    QPixmap pixmap=QPixmap::fromImage(img);
    m_ll->setMinimumSize(img.size());
    m_ll->setPixmap(pixmap);
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
void ZMainWindow::resizeEvent(QResizeEvent *e)
{
    if(m_uartParser)
    {
        m_uartParser->updateCanvasSize(m_ll->size());
    }
    QMainWindow::resizeEvent(e);
}