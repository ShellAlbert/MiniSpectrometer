#ifndef ZSINGLEFRAME_H
#define ZSINGLEFRAME_H

#include <QVector>
#include <QLine>
#include <QPoint>
class ZSingleFrame
{
public:
    //1050-350=700, each frame has maximum 700 lines.
    ZSingleFrame(qint32 capacity=700);
    ~ZSingleFrame();

    qint32 count() const;
    QLine & getLineAt(qint32 i);
    QList<QLine> &getAllLines() const;

private:
    QList<QLine> *m_list;
    qint32 m_capacity;
    QLine m_invalidLine;
};

#endif // ZSINGLEFRAME_H
