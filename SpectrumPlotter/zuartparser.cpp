#include "zuartparser.h"
#include <QPainter>
#include <QFile>
#include <QPoint>
#include <QDebug>
#include <QTextStream>
#include <QFontMetrics>
#include <QPainterPath>
ZUartParser::ZUartParser(ZRingBuffer *buffer, QObject *parent)
    : QObject(parent), m_ringBuffer(buffer)
{
    // Poll the buffer every 10ms to check for complete frames
    m_timer.setInterval(10);
    connect(&m_timer, &QTimer::timeout, this, &ZUartParser::parseLoop);
    m_timer.start();

    m_verbose=0;

    //only keeps 10 history frames.
    m_hisFrame=new ZHistoryFrame(10);

    //color mapping.
    m_gradient=QLinearGradient(0,0,1920,0);
    m_gradient.setColorAt(0.0, Qt::blue);
    m_gradient.setColorAt(0.2, Qt::cyan);
    m_gradient.setColorAt(0.4, Qt::green);
    m_gradient.setColorAt(0.6, Qt::yellow);
    m_gradient.setColorAt(0.8, QColor(255,165,0)); //Orange.
    m_gradient.setColorAt(1.0, Qt::red);
}

ZUartParser::~ZUartParser()
{
    if(m_hisFrame)
    {
        delete m_hisFrame;
        m_hisFrame=nullptr;
    }
}
void ZUartParser::verboseMode(Qt::CheckState state)
{
    if(state==Qt::CheckState::Checked)
    {
        m_verbose=1;
    }else{
        m_verbose=0;
    }
}
void ZUartParser::onZoomIn()
{
    m_yScaleFactor*=1.1f;
}
void ZUartParser::onZoomOut()
{
    m_yScaleFactor/=1.1f;
}
void ZUartParser::updateCanvasSize(QSize newCanvasSize)
{
    m_canvasSize=newCanvasSize;
    m_bSizeChanged=true;
}
void ZUartParser::parseLoop() {
    while (true) {
        // 1. Check if we have enough data for the minimum frame size
        // Min Frame: Header(2) + Len(3) + Type(1) + Payload(x) + Checksum(1) + Tail(2) = 9 bytes
        if (m_ringBuffer->size() < 9) {
            break;
        }

        // 2. Peek at the data to find the Header
        // We peek a small chunk to search for the header
        QByteArray chunk = m_ringBuffer->peek(2);
        if (chunk.isEmpty() || static_cast<unsigned char>(chunk.at(0)) != 0xCC || static_cast<unsigned char>(chunk.at(1)) != 0x81) {
            // No header found at current read position, discard one byte
            m_ringBuffer->consume(1);
            continue;
        }

        // 3. Peek enough bytes to read the Length field and Type.
        // Need at least 2 bytes: Header(2) + Len(3) + Type(1)
        chunk = m_ringBuffer->peek(6);
        if (chunk.size() < 6)
        {
            break;
        }

        //Attention here: totalFrameSize includes all data bytes.
        quint8 len1=static_cast<quint8>(chunk.at(4));
        quint8 len2=static_cast<quint8>(chunk.at(3));
        quint8 len3=static_cast<quint8>(chunk.at(2));

        qint32 totalFrameSize=(static_cast<qint32>(len1)<<16) |
                                (static_cast<qint32>(len2)<<8) |
                                (static_cast<quint32>(len3)<<0) ;

        // 4. Check if the entire frame is available in the buffer
        if (m_ringBuffer->size() < totalFrameSize) {
            break; // Wait for more data
        }

        // 5. Extract the full frame
        QByteArray frame = m_ringBuffer->peek(totalFrameSize);
        if(frame.isEmpty())
        {
            break;
        }

        // 6. Validate Tail
        quint8 tail1=static_cast<unsigned char>(frame.at(frame.size()-2));
        quint8 tail2=static_cast<unsigned char>(frame.at(frame.size()-1));
        if (tail1 != 0x0D || tail2 != 0x0A) {
            emit errorMessage(QString("Invalid tail, discarding frame."));
            m_ringBuffer->consume(1); // Discard header and retry
            continue;
        }

        // // 7. Validate Checksum (XOR of Header + Len + Payload)
        // uint8_t checksum = 0;
        // for (int i = 0; i < totalFrameSize - 2; ++i) { // Exclude Checksum and Tail
        //     checksum ^= static_cast<uint8_t>(frame[i]);
        // }

        // uint8_t receivedChecksum = static_cast<uint8_t>(frame[totalFrameSize - 2]);
        // if (checksum != receivedChecksum) {
        //     qDebug() << "Checksum Error. Expected:" << checksum << "Got:" << receivedChecksum;
        //     m_ringBuffer->consume(1); // Discard header and retry
        //     continue;
        // }

        // 8. Frame is Valid! Process Payload
        // QByteArray payload = frame.mid(6, totalFrameSize-2-3-1-2); // Skip Header(2), Len(3), Checksum(1) and Tail(2).
        // emit frameReceived(payload);
        //dump raw data.
        if(m_verbose){
            QString hexFrame=frame.toHex();
            emit statusMessage(hexFrame);
        }

        //parse different command types.
        quint8 frameType=static_cast<quint8>(frame.at(5));
        switch(frameType)
        {
        case 0x0F:
        {
            quint8 start_high_byte=static_cast<quint8>(frame.at(7));
            quint8 start_low_byte=static_cast<quint8>(frame.at(6));
            qint16 spectrumStart=(static_cast<quint16>(start_high_byte)<<8)|(static_cast<quint16>(start_low_byte)<<0);

            quint8 end_high_byte=static_cast<quint8>(frame.at(9));
            quint8 end_low_byte=static_cast<quint8>(frame.at(8));
            qint16 spectrumEnd=(static_cast<quint16>(end_high_byte)<<8)|(static_cast<quint16>(end_low_byte)<<0);

            emit this->spectrumRange(QString::number(spectrumStart),QString::number(spectrumEnd));
            break;
        }
        case 0x02:
        {
            quint8 temp81, temp82, temp83, temp84;
            quint16 temp16;
            quint32 temp32;
            QString message;

            qint32 index=5; //frameType offset.
            index+=1;
            //exposure state: 1 byte.
            temp81=static_cast<quint8>(frame.at(index));
            if(m_verbose)
            {
                emit statusMessage("Exposure state "+QString::number(temp81,16).toUpper());
            }
            index+=1;
            \
            //exposure time. 4 bytes.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                emit statusMessage("Exposure time "+QString::number(temp32,10).toUpper()+ "us.");
            }
            index+=4;

            //photometric parameters, 27*float.
            //cct.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("cct: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //lux.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("lux: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //ee.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("ee: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r1.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r1: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r2.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r2: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r3.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r3: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r4.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r4: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r5.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r5: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r6.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r6: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r7.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r7: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r8.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r8: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r9.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r9: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r10.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r10: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r11.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r11: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r12.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r12: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r13.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r13: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r14.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r14: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //r15.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("r15: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //ra.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("ra: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //X.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("X: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //Y.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("Y: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //Z.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("Z: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //duv.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("duv: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //u.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("u: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //v.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("v: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //x.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("x: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            //y.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp83=static_cast<quint8>(frame.at(index+2));
            temp84=static_cast<quint8>(frame.at(index+3));
            temp32=(static_cast<quint32>(temp84)<<24)|
                     (static_cast<quint32>(temp83)<<16)|
                     (static_cast<quint32>(temp82)<<8)|
                     (static_cast<quint32>(temp81)<<0);
            if(m_verbose)
            {
                message+=("y: "+QString::number(temp32,16).toUpper()+ ",");
            }
            index+=4;
            if(m_verbose){
                emit statusMessage(message);
            }

            //coefficient. 2 bytes.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp16=(static_cast<quint16>(temp82)<<8)|(static_cast<quint16>(temp81)<<0);
            if(m_verbose)
            {
                emit statusMessage("coefficient: "+QString::number(temp16,10).toUpper());
            }
            index+=2;

            //remember photometric data start offset.
            quint32 photometric_data_offset=index;
            //photometric data. each data is 2 bytes.
            //header(2)+length(3)+type(1)+exposure(1)+exposure time(4)+photometric paramaters(27*4)+coefficient(2)+XXXXX+checksum(1)+tailer(2)
            //2+3+1+1+4+27*4+2+1+2=124
            quint32 photo_data_len=totalFrameSize-124;
            quint32 photo_data_count=photo_data_len/sizeof(quint16);

            //scan all data to find out the maximum value.
            //extend X axis.
            m_maxX=((quint32)(m_canvasSize.width())>photo_data_count)?(m_canvasSize.width()):(photo_data_count);
            m_maxY=0;

            for(quint32 i=0;i<photo_data_count; i++)
            {
                temp81=static_cast<quint8>(frame.at(index+0));
                temp82=static_cast<quint8>(frame.at(index+1));
                temp16=(static_cast<quint16>(temp82)<<8)|(static_cast<quint16>(temp81)<<0);
                if(m_verbose){
                    emit statusMessage(QString::number(i,10)+":"+QString::number(temp16,10));
                }
                index+=2;
                //update maximum Y.
                m_maxY=(temp16>m_maxY)?(temp16):(m_maxY);
            }

            //draw background image and foreground image.
            if(m_backImg.size()!=QSize(m_canvasSize.width()+100,m_canvasSize.height()+100))
            {
                m_backImg=QImage(m_canvasSize.width()+100,m_canvasSize.height()+100,QImage::Format_ARGB32); //extend +100 pixels.
                m_backImg.fill(Qt::black);
                drawBackground(m_backImg);
                emit statusMessage(QString("new background image size: %1x%2").arg(m_maxX).arg(m_maxY));

            }
            if(m_foreImg.size()!=QSize(m_canvasSize.width()+100,m_canvasSize.height()+100))
            {
                m_foreImg=QImage(m_canvasSize.width()+100,m_canvasSize.height()+100,QImage::Format_ARGB32); //extend +100 pixels.
                m_foreImg.fill(Qt::transparent);
                emit statusMessage(QString("new foreground image size: %1x%2").arg(m_maxX).arg(m_maxY));
            }
            drawForeground(m_foreImg,photo_data_count,m_maxY,frame,photometric_data_offset,photo_data_count);

            //emit to UI thread.
            //Warning Here!!!!
            //the coordinate (0,0) is located at the top-left corner!!!!!
            //x axis increases torward to right.
            //y axis increases torward to down.
            QRect validRect(0,65535-m_maxY,(1050-350),m_maxY);
            emit newImage(m_backImg,m_foreImg,validRect);

            //checksum. 1 byte.
            temp81=static_cast<quint8>(frame.at(index+0));
            if(m_verbose)
            {
                emit statusMessage("checksum: "+QString::number(temp81,16).toUpper());
            }
            index+=1;

            //tailer. 2 bytes.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp16=(static_cast<quint16>(temp81)<<8)|(static_cast<quint16>(temp82)<<0);
            if(m_verbose)
            {
                emit statusMessage("tailer: "+QString::number(temp16,16).toUpper());
            }
            break;
        }
        default:
            emit errorMessage("Unknown command type "+QString::number(frameType,16).toUpper()+","+frame.toHex());
            break;
        }

        // 9. Remove the processed frame from the buffer
        m_ringBuffer->consume(totalFrameSize);
    }
}
//call this function once when window size changes(resize/init), not every frame.
void ZUartParser::drawBackground(QImage &img)
{
    QFont font("DejaVu Serif",12);
    QFontMetrics fm(font);

    QPainter painter(&img);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(Qt::red,2,Qt::SolidLine));
    painter.setFont(font);

    //by default, the Qt's coordinate (0,0) is located at the left-top corner.
    //here use translate to move (0,0) to (70, img.height()-70), thus right-bottom corner.
    //we extended the image size by 100 pixels in width and height.
    //so here 70 pixels are within its valid range.
    painter.translate(70, img.height()-70);

    //draw horizontal legend.
    QString strHorizontal("Supported waveform range (nanometer scale 350nm ~ 1050nm)");
    qint32 strHorizontalWidth=fm.horizontalAdvance(strHorizontal); // /2.
    painter.setPen(QPen(Qt::white,2,Qt::DotLine));
    painter.drawText((img.width()-strHorizontalWidth)/2,60,strHorizontal);

    //draw vertical legend.
    QString strVertical("Light Intensity ( 0 ~ 2^16-1 ) 100% Normalization");
    painter.rotate(-90);
    painter.drawText((img.height()-fm.horizontalAdvance(strVertical))/2,-50,strVertical);
    // painter.drawText(100,-50,strVertical);
    painter.rotate(90);

    // Flip the Y-axis so positive Y goes UP
    painter.scale(1, -1);
    painter.setPen(QPen(Qt::red,2,Qt::SolidLine));

    //x axis draw line from (0,0) to (1000,0).
    painter.drawLine(QPoint(0,0),QPoint(img.width()-20,0));
    //draw arrow.
    //painter.drawLine(QPoint(img.width()-20-200,0),QPoint(img.width()-20-250,25)); //because image was extended +100 pixels.
    //painter.drawLine(QPoint(img.width()-20-200,0),QPoint(img.width()-20-250,-25));
    //y axis draw line from (0,0) to (0,65535).
    painter.drawLine(QPoint(0,0),QPoint(0,img.height()-20));
    //draw arrow.
    //painter.drawLine(QPoint(0,img.height()-20-100),QPoint(-25,img.height()-20-150));
    //painter.drawLine(QPoint(0,img.height()-20-100),QPoint(25,img.height()-20-150));

    //draw scale every 10 pixels. //(1050-350)/10=700/10=70.
    //350,360,370,380,390,400,410,420,430,440,.......1050.
    //totally draw 70 thick lines.
    qint32 pixelGap=(img.width()-100)/70; //subtract extended 100 pixels.
    for(qint32 i=0, x_nm_begin=350; i<=70; i++)
    {
        qint32 xDest=i*pixelGap;
        if(0==i) //(0,0)
        {
            //draw x axis text.
            painter.save();
            painter.translate(0,-10);
            painter.scale(1, -1);
            painter.rotate(-45.0);
            int halfWidth=fm.horizontalAdvance(QString::number(x_nm_begin,10))/2;
            int halfHeight=fm.height()/2;
            int yOffset=fm.ascent()/2-fm.descent()/2;
            painter.setPen(QPen(Qt::white,2,Qt::DotLine));
            painter.setRenderHint(QPainter::Antialiasing);
            painter.drawText(-halfWidth,/*yOffset*/20,QString::number(x_nm_begin,10));
            x_nm_begin+=10;
            painter.restore();

        }else{
            //draw thick lines.
            painter.setPen(QPen(Qt::red,2,Qt::SolidLine));
            painter.drawLine(QPoint(xDest,0),QPoint(xDest,15));

            //draw text.
            painter.save();
            painter.translate(xDest,-10);
            painter.scale(1, -1);
            painter.rotate(-45.0);
            int halfWidth=fm.horizontalAdvance(QString::number(x_nm_begin,10))/2;
            int halfHeight=fm.height()/2;
            int yOffset=fm.ascent()/2-fm.descent()/2;
            painter.setPen(QPen(Qt::white,2,Qt::DotLine));
            painter.setRenderHint(QPainter::Antialiasing);
            painter.drawText(-halfWidth,/*yOffset*/20,QString::number(x_nm_begin,10));
            x_nm_begin+=10;
            painter.restore();
        }
    }
    //draw y axis. only draw 10 gaps totally.
    //mean light intensity ranges from 0~100, each gap is 10.
    qint32 gapYAxis=(img.height()-100)/10; //subtract extended 100 pixels.
    for(qint32 i=0,y_nm_begin=0; i<=10; i++)
    {
        qint32 yDest=i*gapYAxis;
        if(0==yDest) //(0,0)
        {
            painter.save();
            painter.translate(-30,0);
            painter.scale(1, -1);
            painter.rotate(-30.0);
            painter.setPen(QPen(Qt::white,2,Qt::DotLine));
            painter.setRenderHint(QPainter::Antialiasing);
            painter.drawText(0,0,QString("0"));
            y_nm_begin+=10;
            painter.restore();
        }else{
            //draw thick lines.
            painter.setPen(QPen(Qt::red,2,Qt::SolidLine));
            painter.drawLine(QPoint(-10,yDest),QPoint(0,yDest));

            //draw text.
            painter.save();
            painter.translate(-30,yDest);
            painter.scale(1, -1);
            painter.rotate(-30.0);
            painter.setPen(QPen(Qt::white,2,Qt::DotLine));
            painter.setRenderHint(QPainter::Antialiasing);
            painter.drawText(0,0,QString::number(y_nm_begin,10));
            y_nm_begin+=10;
            painter.restore();
        }
    }
}
//call this function to render new image when every new frame comes.
void ZUartParser::drawForeground(QImage &img, qint32 max_x, qint32 max_y, const QByteArray &data_array, quint32 index, quint32 point_count)
{
    //update the oldest frame in history.
    ZSingleFrame *frame=m_hisFrame->getOldest();

    //move (0,0) to a new position.
    //so we can only update the specified area. (points area) to save time while painting.
    QPainter painter(&img);
    painter.setPen(QPen(Qt::green,2,Qt::SolidLine));

    //the image was extended by 100 pixels in width and height for axes drawing.
    //so here 70 pixels are acceptable, they are within the valid range.
    painter.translate(70, img.height()-70);

    // Flip the Y-axis so positive Y goes UP.
    painter.scale(1, -1);

    //calculate scale factor to normalize data before drawing.
    //if we use QImage::scaled() function, the final image will be blurred.
    if(m_bSizeChanged) //optimization, only calculate once if size changed.
    {
        m_xScaleFactor=(img.width()-100)/(qreal)(max_x);
        m_yScaleFactor=(img.height()-100)/(qreal)(max_y);
        m_bSizeChanged=false;
    }

    // QVector<QPoint> points;
    // QVector<QLine> lines;
    QPoint pt1;
    quint32 array_index=index;
    qint32 lineIdx=0;
    for(quint32 i=0;i<point_count; i++)
    {
        quint8 temp81=static_cast<quint8>(data_array.at(array_index+0));
        quint8 temp82=static_cast<quint8>(data_array.at(array_index+1));
        quint16 temp16=(static_cast<quint16>(temp82)<<8)|(static_cast<quint16>(temp81)<<0);
        array_index+=2;

        //original.
        // QPoint pt2=QPoint(i,temp16);
        // points.append(pt2);
        // if(i!=0)
        // {
        //     lines.append(QLine(pt1,pt2));
        // }
        // pt1=pt2;

        //scale image in working thread to prevent UI zombie state.
        QPoint pt3=QPoint(i*m_xScaleFactor,temp16*m_yScaleFactor);
        //statusMessage(QString("(%1,%2)*(%3,%4)=(%5,%6)").arg(pt2.x()).arg(pt2.y()).arg(xScaleFactor).arg(yScaleFactor).arg(pt3.x()).arg(pt3.y()));
        // points.append(pt3);
        if(i!=0 && lineIdx<frame->count()) //at first, no need to draw line.
        {
            QLine &line=frame->getLineAt(lineIdx);
            line.setPoints(pt1,pt3);
            ++lineIdx;
        }
        pt1=pt3;
    }

    //clear foreground image and draw the latest 10 frames.
    m_foreImg.fill(Qt::transparent);
    for(qint32 i=0;i<m_hisFrame->count();i++)
    {
        ZSingleFrame *myFrame=m_hisFrame->getFrameAt(i);
        //drawLines() is faster than drawLine(), high efficiency.
        //painter.drawLines(myFrame->getAllLines());
        ///////////////////////////////////////////////////////////
        QList<QLine> &allLines=myFrame->getAllLines();
        QList<QPoint> allPoint;
        for(qint32 i=0;i<allLines.count();i++)
        {
            allPoint.append(allLines[i].p1());
            allPoint.append(allLines[i].p2());
        }
        //make it enclose.
        painter.setBrush(m_gradient);
        painter.drawPolygon(allPoint);

        // for(qint32 j=0;j<myFrame->count();j++)
        // {
        //     QLine &line=myFrame->getLineAt(j);
        //     painter.drawLine(line);
        // }
    }
    painter.end();

    // QFile file("read_spectrum.dat");
    // if(file.open(QIODevice::WriteOnly|QIODevice::Text))
    // {
    //     QTextStream ts(&file);
    //     for(qint32 i=0;i<points.size();i++)
    //     {
    //         ts<<points.at(i).x()<<" "<<points.at(i).y()<<"\n";
    //     }
    //     file.close();
    // }
}