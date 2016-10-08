#ifndef DISCARDTHREAD_H
#define DISCARDTHREAD_H

#include <QThread>
#include <QStringList>

class DiscardThread : public QThread
{
    Q_OBJECT
public:
    DiscardThread(QStringList devs);

    QStringList _blockDevs;
    void run();
signals:
    void finished();
    void error(QString err);

private:
    bool discard(QString blkdev, qint64 start, qint64 end);
};

#endif // DISCARDTHREAD_H
