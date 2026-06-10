#ifndef ZMAINWINDOW_H
#define ZMAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QLineEdit>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QToolButton>
#include <QCheckBox>
#include <QScrollArea>
#include <QSplitter>
#include <QLabel>
#include <QTimer>
#include "zringbuffer.h"
#include "zuartparser.h"
#include "zuartworker.h"

class ZMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ZMainWindow(QWidget *parent = nullptr);
    ~ZMainWindow() override;

signals:
    void sendCommand(const QByteArray &command);
public slots:
    void onSpectrumRange(const QString &start, const QString &end);

    void onMessage(const QString &message);
    void onNewImage(const QImage &backgroundImg, const QImage &foregroundImage,const QRect &validRect);

    void onConnectClicked();
    void onSendClicked();
    void onGetSpectrum();
    void onUpdateUI(const QByteArray &data);

    void onPortStatusChanged(bool isOpen);

protected:
    void resizeEvent(QResizeEvent *e) override;
private slots:
    void onPeriodically(Qt::CheckState state);
    void onTimeout();
private:
    QThread *m_workerThread;
    ZRingBuffer *m_ringBuffer;
    ZUartWorker *m_uartWorker;
    ZUartParser *m_uartParser;

private:
    QVBoxLayout *m_vLayoutMain;
    QLabel *m_ll;
    QScrollArea *m_scrollArea;
    QVBoxLayout *m_vlayoutScrollArea;

    QHBoxLayout *m_hLayoutBtn;
    QToolButton *m_tbInitDev;
    QToolButton *m_tbGetRange;
    QToolButton *m_tbGetSpectrum;
    QCheckBox *m_cbVerbose;
    QCheckBox *m_cbPeriodically;
    QLabel *m_llCounts;
    QTextEdit *m_teLog;

    QTimer *m_timer;

    QSplitter *m_spliter;
    //how many times does periodically run?
    quint32 m_cntPeriodically;

};
#endif // ZMAINWINDOW_H
