#include "zhistoryframe.h"

//only keeps the latest 10 single frames, each frame has maximum 700(1050nm-350nm) lines.
ZHistoryFrame::ZHistoryFrame(int capacity):
    m_capacity(capacity),m_index(0)
{
    for(qint32 i=0;i<capacity;i++)
    {
        ZSingleFrame *frame=new ZSingleFrame(700);
        m_list.append(frame);
    }
}
ZHistoryFrame::~ZHistoryFrame()
{
    for(qint32 i=0;i<m_capacity;i++)
    {
        ZSingleFrame *frame=m_list.takeFirst();
        delete frame;
        frame=nullptr;
    }
    m_list.clear();
}
ZSingleFrame* ZHistoryFrame::getOldest()
{
    ZSingleFrame* frame=m_list[m_index];
    //move read pointer circularly.
    m_index=(m_index+1)%m_capacity;
    return frame;
}
ZSingleFrame* ZHistoryFrame::getFrameAt(qint32 i)
{
    if(i>=0 && i<m_capacity)
    {
        return m_list.at(i);
    }
    return nullptr;
}
qint32 ZHistoryFrame::count() const
{
    return m_list.size();
}