#ifndef ZSPECTRUMCANVAS_H
#define ZSPECTRUMCANVAS_H
#include <QObject>
#include <QWidget>
#include <QPaintEvent>
#include <QByteArray>
#include <zringbuffer.h>
class ZSpectrumCanvas : public QWidget
{
    Q_OBJECT
public:
    //supported range: 350nm~1050nm. 1050-350=700
    ZSpectrumCanvas(qint32 capacity=700);
    ~ZSpectrumCanvas();
public slots:
protected:
    void paintEvent(QPaintEvent *e);
private:
    ZRingBuffer *m_ringBuffer;
};

#endif // ZSPECTRUMCANVAS_H
