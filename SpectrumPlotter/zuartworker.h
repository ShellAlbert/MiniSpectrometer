#ifndef ZUARTWORKER_H
#define ZUARTWORKER_H

#include <QObject>
#include <QSerialPort>
#include "zringbuffer.h"

class ZUartWorker : public QObject {
    Q_OBJECT
public:
    explicit ZUartWorker(ZRingBuffer *buffer, QObject *parent = nullptr);
    ~ZUartWorker();


public slots:
    void initPort(const QString &portName, qint32 baudRate);
    void sendData(const QByteArray &data);

private slots:
    void onReadyRead();

signals:
    void statusMessage(const QString &msg);
    void errorMessage(const QString &msg);
private:
    void writeData(const QByteArray &data);
private:
    ZRingBuffer *m_ringBuffer;
    QSerialPort *m_serialPort;
};

#endif // UARTWORKER_H