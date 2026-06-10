#include "zuartworker.h"


ZUartWorker::ZUartWorker(ZRingBuffer *buffer, QObject *parent)
    : QObject(parent), m_ringBuffer(buffer), m_serialPort(nullptr)
{
    m_verbose=0;
}

ZUartWorker::~ZUartWorker() {
    if (m_serialPort) {
        m_serialPort->close();
        delete m_serialPort;
    }
}

void ZUartWorker::initPort(const QString &portName, qint32 baudRate) {
    m_serialPort = new QSerialPort(this);
    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(baudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        emit statusMessage(QString("Device %1 opened successfully.").arg(portName));
        // Connect readyRead signal to our slot
        connect(m_serialPort, &QSerialPort::readyRead, this, &ZUartWorker::onReadyRead);
    } else {
        emit errorMessage("Failed to open port: " + m_serialPort->errorString());
    }
}

void ZUartWorker::onReadyRead() {
    if (!m_serialPort) {
        emit errorMessage("Open device before reading.");
        return;
    }

    QByteArray data = m_serialPort->readAll();
    if (!data.isEmpty()) {
        // Push data to Ring Buffer
        if (!m_ringBuffer->write(data)) {
            emit errorMessage("Ring Buffer Overflow! Data lost.");
        }
    }
}
void ZUartWorker::sendData(const QByteArray &data)
{
    writeData(data);
    if(m_verbose)
    {
        emit statusMessage(data.toHex());
    }
}
void ZUartWorker::verboseMode(Qt::CheckState state)
{
    if(state==Qt::CheckState::Checked)
    {
        m_verbose=1;
    }else{
        m_verbose=0;
    }
}
void ZUartWorker::writeData(const QByteArray &data)
{
    if(m_serialPort && m_serialPort->isOpen())
    {
        qint64 byteWritten=m_serialPort->write(data);
        if(byteWritten==-1)
        {
            emit errorMessage("Write failed:"+m_serialPort->errorString());
        }else{
            //flush to ensure data is send immediately.
            m_serialPort->flush();
        }
    }else{
        emit errorMessage("Port not open, cannot write.");
    }
}