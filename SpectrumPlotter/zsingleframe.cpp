#include "zsingleframe.h"
//1050-350=700, each frame has maximum 700 lines.
ZSingleFrame::ZSingleFrame(qint32 capacity)
{
    m_capacity=capacity;
    m_list=new QList<QLine>;
    for(qint32 i=0;i<capacity; i++)
    {
        QLine line(0,0,0,0);
        m_list->append(line);
    }
}
ZSingleFrame::~ZSingleFrame()
{
    m_list->clear();
    delete m_list;
    m_list=nullptr;
}
qint32 ZSingleFrame::count() const
{
    return m_list->count();
}
QLine & ZSingleFrame::getLineAt(qint32 i)
{
    if(i>=0 && i<m_list->count())
    {
        return (*m_list)[i];
    }
    return m_invalidLine;
}
QList<QLine> & ZSingleFrame::getAllLines() const
{
    return (*m_list);
}