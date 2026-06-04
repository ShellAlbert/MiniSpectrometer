#ifndef ZUARTPARSER_H
#define ZUARTPARSER_H

#include <QObject>
#include <QTimer>
#include <QByteArray>
#include <QImage>
#include <QDebug>
#include "zringbuffer.h"

class ZUartParser : public QObject {
    Q_OBJECT
public:
    explicit ZUartParser(ZRingBuffer *buffer, QObject *parent = nullptr);

public slots:
    void verboseMode(Qt::CheckState state);
private slots:
    void parseLoop();

signals:
    void errorMessage(const QString &message);
    void statusMessage(const QString &message);
    void frameReceived(const QByteArray &payload);
    void spectrumRange(const QString &start, const QString &end);
    void newImage(const QImage &image);
private:
    void drawImage(qint32 width, qint32 height, const QByteArray &data_array, quint32 index, quint32 point_count);
private:
    ZRingBuffer *m_ringBuffer;
    QTimer m_timer;
    QImage m_image;

    quint8 m_verbose;
};

#endif // PROTOCOLPARSER_H