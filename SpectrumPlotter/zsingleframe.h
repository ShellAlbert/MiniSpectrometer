#ifndef ZSINGLEFRAME_H
#define ZSINGLEFRAME_H

#include <QVector>
#include <QLine>
#include <QPoint>
class ZSingleFrame
{
public:
    ZSingleFrame();
    ~ZSingleFrame();

    void addLine(const QLine &line);
    void clear();
    const QVector<QLine>& getLines() const;

    qint32 count() const;

private:
    QVector<QLine> m_vector;
};

#endif // ZSINGLEFRAME_H
