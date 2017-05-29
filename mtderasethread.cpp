#include "mtderasethread.h"

#include <QProcess>
#include <QDebug>

MtdEraseThread::MtdEraseThread(QStringList devs) :
    _mtdDevs(devs)
{
}

void MtdEraseThread::run() {
    foreach (const QString &str, _mtdDevs) {
        if (!erase("/dev/" + str, 0, 0))
            break;
    }
    emit finished();
}

bool MtdEraseThread::erase(QString mtddev, qint64 start, qint64 end)
{
    QStringList args;

    /* If start and end is zero, flash_erase will erase the whole raw NAND */
    args << mtddev << QString::number(start) << QString::number(end);

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start("/usr/sbin/flash_erase", args);
    p.waitForFinished(-1);

    qDebug() << "flash_erase of device" << mtddev << "finished with exit code" << p.exitCode();

    if (p.exitCode() != 0) {
        emit error(tr("Eraseing device %1 failed").arg(mtddev) + "\n" + p.readAll());
        return false;
    }
    return true;
}
