#include "discardthread.h"
#include "multiimagewritethread.h"

#include <QProcess>
#include <QDebug>

DiscardThread::DiscardThread(QStringList devs) :
    _blockDevs(devs)
{
}

void DiscardThread::run() {
    foreach (const QString &str, _blockDevs) {
        if (!discard("/dev/" + str, 0, 0))
            break;
    }
    emit finished();
}

bool DiscardThread::discard(QString blkdev, qint64 start, qint64 end)
{
    QByteArray output;

    if (!MultiImageWriteThread::eraseBlockDevice(blkdev.toAscii(), start, end, output)) {
        emit error(tr("Discarding content on device %1 failed").arg(blkdev) + "\n" + output);
        return false;
    }

    return true;
}
