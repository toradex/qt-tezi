#ifndef MTDERASETHREAD_H
#define MTDERASETHREAD_H

#include <QThread>
#include <QStringList>

class MtdEraseThread : public QThread
{
    Q_OBJECT
public:
    MtdEraseThread(QStringList devs);

    void run();
signals:
    void finished();
    void error(QString err);

private:
    QStringList _mtdDevs;
    bool erase(QString mtddev);
};

#endif // MTDERASETHREAD_H
