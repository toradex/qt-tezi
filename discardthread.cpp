#include "discardthread.h"

#include <QProcess>
#include <QDebug>

DiscardThread::DiscardThread(QStringList devs) :
    _blockDevs(devs)
{
}

void DiscardThread::run() {
    foreach (const QString &str, _blockDevs) {
        if (!discard(str, 0, 0))
            break;
    }
    emit finished();
}

bool DiscardThread::discard(QString blkdev, qint64 start, qint64 end)
{
    QStringList args;

    /* If start and end is zero, blkdiscard will discard the whole eMMC */
    if (start)
        args.append(QString("-o %1").arg(start));
    if (end)
        args.append(QString("-l %1").arg(end));
    args.append(blkdev);

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("/usr/sbin/blkdiscard", args);
    p.waitForFinished(-1);

    qDebug() << "blkdiscard of device" << blkdev << "finished with exit code" << p.exitCode();

    if (p.exitCode() != 0) {
        emit error(tr("Discarding device %1 failed").arg(blkdev) + "\n" + p.readAll());
        return false;
    }
    return true;
}
