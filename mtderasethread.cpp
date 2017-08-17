#include "mtderasethread.h"
#include "multiimagewritethread.h"

#include <QProcess>
#include <QDebug>

MtdEraseThread::MtdEraseThread(QStringList devs) :
    _mtdDevs(devs)
{
}

void MtdEraseThread::run() {
    foreach (const QString &str, _mtdDevs) {
        if (!erase("/dev/" + str))
            break;
    }
    emit finished();
}

bool MtdEraseThread::erase(QString mtddev)
{
    if (!MultiImageWriteThread::eraseMtdDevice(mtddev.toAscii())) {
        emit error(tr("Erasing device %1 failed").arg(mtddev));
        return false;
    }
    return true;
}
