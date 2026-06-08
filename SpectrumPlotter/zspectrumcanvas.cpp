#include "zspectrumcanvas.h"
#include <new> //for std::bad_alloc
ZSpectrumCanvas::ZSpectrumCanvas(qint32 capacity)
{
    try{
        m_ringBuffer=new ZRingBuffer(capacity);
    }catch(const std::bad_alloc &e)
    {
        m_ringBuffer=nullptr;
        throw;
    }
}
ZSpectrumCanvas::~ZSpectrumCanvas()
{

}
void ZSpectrumCanvas::paintEvent(QPaintEvent *e)
{
    Q_UNUSED(e);
    //QPainter painter(this);

}