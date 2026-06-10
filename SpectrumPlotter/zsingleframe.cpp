#include "zsingleframe.h"

ZSingleFrame::ZSingleFrame()
{

}
ZSingleFrame::~ZSingleFrame()
{
    clear();
}
void ZSingleFrame::addLine(const QLine &line)
{
    m_vector.append(line);
}

void ZSingleFrame::clear()
{
    m_vector.clear();
}
const QVector<QLine>& ZSingleFrame::getLines() const
{
    return m_vector;
}
qint32 ZSingleFrame::count() const
{
    return m_vector.size();
}