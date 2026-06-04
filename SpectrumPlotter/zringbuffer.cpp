#include "zringbuffer.h"


ZRingBuffer::ZRingBuffer(int capacity)
    : m_capacity(capacity), m_readPos(0), m_writePos(0), m_count(0)
{
    m_buffer.resize(capacity);
}

// Write data to the buffer (Called by UART Reader Thread)
bool ZRingBuffer::write(const QByteArray &data) {
    if (data.size() > freeSpace()) {
        return false; // Buffer overflow protection
    }

    QMutexLocker locker(&m_mutex);
    for (char byte : data) {
        m_buffer[m_writePos] = byte;
        m_writePos = (m_writePos + 1) % m_capacity;
        ++m_count;
    }
    return true;
}

// Read available data without removing it (Peek)
QByteArray ZRingBuffer::peek(int size) const {
    QMutexLocker locker(&m_mutex);
    if (size > m_count) size = m_count;
    if (size <= 0) return QByteArray();

    QByteArray result;
    result.reserve(size);

    int currentRead = m_readPos;
    for (int i = 0; i < size; ++i) {
        result.append(m_buffer[currentRead]);
        currentRead = (currentRead + 1) % m_capacity;
    }
    return result;
}

// Remove data from the buffer (Consume)
void ZRingBuffer::consume(int size) {
    QMutexLocker locker(&m_mutex);
    if (size > m_count) size = m_count;

    m_readPos = (m_readPos + size) % m_capacity;
    m_count -= size;
}

int ZRingBuffer::size() const {
    QMutexLocker locker(&m_mutex);
    return m_count;
}

int ZRingBuffer::freeSpace() const {
    QMutexLocker locker(&m_mutex);
    return m_capacity - m_count;
}