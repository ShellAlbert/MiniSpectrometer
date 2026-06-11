#ifndef ZHISTORYFRAME_H
#define ZHISTORYFRAME_H
#include <QList>
#include <QLine>
#include "zsingleframe.h"

class ZHistoryFrame
{
public:
    //only keeps the latest 10 single frames.
    ZHistoryFrame(int capacity=10);
    ~ZHistoryFrame();

    ZSingleFrame* getOldest();
    ZSingleFrame* getFrameAt(qint32 i);
    qint32 count() const;
private:
    QList<ZSingleFrame*> m_list;
    int m_capacity;
    int m_index;
};

#endif // ZHISTORYFRAME_H
