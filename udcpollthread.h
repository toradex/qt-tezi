#ifndef UDCPOLLTHREAD_H
#define UDCPOLLTHREAD_H

#include <QObject>
#include <QThread>

class UdcPollThread : public QThread
{
    Q_OBJECT

public:
    UdcPollThread(QObject *parent);

    void run();

signals:
    void finished();
};

#endif // UDCPOLLTHREAD_H
