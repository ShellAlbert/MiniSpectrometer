#ifndef ZRINGBUFFER_H
#define ZRINGBUFFER_H

#include <QObject>
#include <QByteArray>
#include <QMutex>
#include <QSemaphore>

class ZRingBuffer : public QObject
{
    Q_OBJECT
public:
    explicit ZRingBuffer(int capacity = 4096);

    // Write data to the buffer (Called by UART Reader Thread)
    bool write(const QByteArray &data);

    // Read available data without removing it (Peek)
    QByteArray peek(int size) const;

    // Remove data from the buffer (Consume)
    void consume(int size);

    int size() const;

    int freeSpace() const;

private:
    QByteArray m_buffer;
    const int m_capacity;
    int m_readPos;
    int m_writePos;
    int m_count;
    mutable QMutex m_mutex;

};

#endif // ZRINGBUFFER_H
