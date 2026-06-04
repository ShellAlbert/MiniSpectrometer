#ifndef ZMAINWINDOW_H
#define ZMAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QLineEdit>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QToolButton>
#include <QCheckBox>
#include <QLabel>
#include "zringbuffer.h"
#include "zuartparser.h"
#include "zuartworker.h"

class ZMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ZMainWindow(QWidget *parent = nullptr);
    ~ZMainWindow() override;

public slots:
    void onSpectrumRange(const QString &start, const QString &end);


    void onMessage(const QString &message);
    void onNewImage(const QImage &image);
    void onNewFrame(const QByteArray &payload);


    void onConnectClicked();
    void onSendClicked();
    void onGetSpectrum();
    void onUpdateUI(const QByteArray &data);

    void onPortStatusChanged(bool isOpen);
private:
    QThread *m_workerThread;
    ZRingBuffer *m_ringBuffer;
    ZUartWorker *m_uartWorker;
    ZUartParser *m_uartParser;

private:
    QVBoxLayout *m_vLayoutMain;
    QLabel *m_ll;
    QHBoxLayout *m_hLayoutBtn;
    QToolButton *m_tbInitDev;
    QToolButton *m_tbGetRange;
    QToolButton *m_tbGetSpectrum;
    QCheckBox *m_cbVerbose;
    QTextEdit *m_teLog;
};
#endif // ZMAINWINDOW_H
