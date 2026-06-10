#ifndef ZUARTPARSER_H
#define ZUARTPARSER_H

#include <QObject>
#include <QTimer>
#include <QByteArray>
#include <QImage>
#include <QDebug>
#include "zringbuffer.h"
#include "zsingleframe.h"

class ZUartParser : public QObject {
    Q_OBJECT
public:
    explicit ZUartParser(ZRingBuffer *buffer, QObject *parent = nullptr);
\
    void updateCanvasSize(QSize newCanvasSize);

public slots:
    void verboseMode(Qt::CheckState state);
private slots:
    void parseLoop();

signals:
    void errorMessage(const QString &message);
    void statusMessage(const QString &message);
    void frameReceived(const QByteArray &payload);
    void spectrumRange(const QString &start, const QString &end);
    void newImage(const QImage &backgroundImg,const QImage &foregroundImage, const QRect &validRect);
private:
    void drawBackground(QImage &img);
    void drawForeground(QImage &img, qint32 max_x, qint32 max_y, const QByteArray &data_array, quint32 index, quint32 point_count);
private:
    ZRingBuffer *m_ringBuffer;
    QTimer m_timer;

    quint8 m_verbose;
    quint32 m_cntSpectrum;

    //canvas size changed.
    //scale image to adapt to new canvas.
    QSize m_canvasSize;

    //the mimum and maximum of X and Y in single image.
    qint32 m_maxX;
    qint32 m_maxY;

    QImage m_backImg;
    QImage m_foreImg;

    //curve history.
    QVector<ZSingleFrame> m_vector;
};

#endif // PROTOCOLPARSER_H