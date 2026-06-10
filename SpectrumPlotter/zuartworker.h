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
    void verboseMode(Qt::CheckState state);

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
    qint32 m_verbose;
};

#endif // UARTWORKER_H