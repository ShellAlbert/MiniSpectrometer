#include "zuartparser.h"
#include <QPainter>
#include <QFile>
#include <QTextStream>
ZUartParser::ZUartParser(ZRingBuffer *buffer, QObject *parent)
    : QObject(parent), m_ringBuffer(buffer)
{
    //supported range: 350nm~1050nm
    //x axis range is 1050-350=700.
    //y axis range is 0~0xFFFF.
    m_image=QImage(700,10000,QImage::Format_ARGB32);
    m_image.fill(Qt::black);

    // Poll the buffer every 10ms to check for complete frames
    m_timer.setInterval(10);
    connect(&m_timer, &QTimer::timeout, this, &ZUartParser::parseLoop);
    m_timer.start();

    m_verbose=0;
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
            emit statusMessage("Exposure state "+QString::number(temp81,16).toUpper());
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
            emit statusMessage("Exposure time "+QString::number(temp32,10).toUpper()+ "us.");
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
            message+=("cct: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("lux: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("ee: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r1: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r2: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r3: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r4: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r5: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r6: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r7: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r8: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r9: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r10: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r11: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r12: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r13: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r14: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("r15: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("ra: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("X: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("Y: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("Z: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("duv: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("u: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("v: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("x: "+QString::number(temp32,16).toUpper()+ ",");
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
            message+=("y: "+QString::number(temp32,16).toUpper()+ ",");
            index+=4;
            if(m_verbose){
                emit statusMessage(message);
            }

            //coefficient. 2 bytes.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp16=(static_cast<quint16>(temp82)<<8)|(static_cast<quint16>(temp81)<<0);
            emit statusMessage("coefficient: "+QString::number(temp16,10).toUpper());
            index+=2;

            //remember photometric data start offset.
            quint32 photometric_data_offset=index;
            //photometric data. each data is 2 bytes.
            //header(2)+length(3)+type(1)+exposure(1)+exposure time(4)+photometric paramaters(27*4)+coefficient(2)+XXXXX+checksum(1)+tailer(2)
            //2+3+1+1+4+27*4+2+1+2=124
            quint32 photo_data_len=totalFrameSize-124;
            quint32 photo_data_count=photo_data_len/sizeof(quint16);

            //scan all data to find out the maximum value.
            quint16 maxValue=0;

            for(quint32 i=0;i<photo_data_count; i++)
            {
                temp81=static_cast<quint8>(frame.at(index+0));
                temp82=static_cast<quint8>(frame.at(index+1));
                temp16=(static_cast<quint16>(temp82)<<8)|(static_cast<quint16>(temp81)<<0);
                if(m_verbose){
                    emit statusMessage(QString::number(i,10)+":"+QString::number(temp16,10));
                }
                index+=2;

                maxValue=(temp16>maxValue)?(temp16):(maxValue);
            }
            drawImage(photo_data_count,maxValue,frame,photometric_data_offset,photo_data_count);

            //checksum. 1 byte.
            temp81=static_cast<quint8>(frame.at(index+0));
            emit statusMessage("checksum: "+QString::number(temp81,16).toUpper());
            index+=1;

            //tailer. 2 bytes.
            temp81=static_cast<quint8>(frame.at(index+0));
            temp82=static_cast<quint8>(frame.at(index+1));
            temp16=(static_cast<quint16>(temp81)<<8)|(static_cast<quint16>(temp82)<<0);
            emit statusMessage("tailer: "+QString::number(temp16,16).toUpper());
            break;
        }
        default:
            emit errorMessage("Unknown command type "+QString::number(frameType,16).toUpper()+".");
            break;
        }

        // 9. Remove the processed frame from the buffer
        m_ringBuffer->consume(totalFrameSize);
    }
}
void ZUartParser::drawImage(qint32 width, qint32 height, const QByteArray &data_array, quint32 index, quint32 point_count)
{
    // QImage image(width+100,height+100,QImage::Format_ARGB32);
    // image.fill(Qt::white);

    QPainter painter(&m_image);
    painter.setPen(QPen(Qt::red,6,Qt::SolidLine));
    // 1. Translate origin so that logical (0,0) is at pixel (0, height - 100)
    // This effectively makes the bottom of the image correspond to logical y = -100
    painter.translate(0, m_image.height() - 100);

    // 2. Flip the Y-axis so positive Y goes UP
    painter.scale(1, -1);

    QVector<QPoint> points;
    QVector<QLine> lines;
    QPoint pt1;
    quint32 data_index=index;
    for(quint32 i=0;i<point_count; i++)
    {
        quint8 temp81, temp82;
        quint16 temp16;
        temp81=static_cast<quint8>(data_array.at(data_index+0));
        temp82=static_cast<quint8>(data_array.at(data_index+1));
        temp16=(static_cast<quint16>(temp82)<<8)|(static_cast<quint16>(temp81)<<0);
        data_index+=2;
        QPoint pt2=QPoint(i,temp16);
        points.append(pt2);
        if(i!=0)
        {
            lines.append(QLine(pt1,pt2));
        }
        pt1=pt2;
    }
    //painter.drawPoints(points.data(),points.size());
    painter.drawLines(lines);
    painter.end();
    emit newImage(m_image);

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