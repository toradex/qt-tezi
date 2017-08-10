#include "mtdnamedevicetranslator.h"

#include <QByteArray>
#include <QDebug>
#include <QFile>
#include <QString>
#include <QStringList>

MtdNameDeviceTranslator::MtdNameDeviceTranslator(QObject *parent) : QObject(parent),
  _nameDeviceMap()
{
    QFile mtdList("/proc/mtd");
    if (!mtdList.open(QFile::ReadOnly)) {
        qDebug() << "Failed to read MTD device list!";
        return;
    }

    QTextStream in(&mtdList);
    QString line = in.readLine();
    while (!line.isNull()) {
        // Parse line such as:
        // mtd1: 00180000 00020000 "u-boot1"
        QStringList mtdentry = line.split(':');
        QString mtddev = mtdentry[0];
        QStringList mtdinfo = mtdentry[1].split(' ', QString::SkipEmptyParts);
        // Remove quotes
        QString mtdname = mtdinfo[2].mid(1, mtdinfo[2].length() - 2);
        _nameDeviceMap.insert(mtdname, mtddev);

        line = in.readLine();
    }

}

QString MtdNameDeviceTranslator::translate(QString &name)
{
    return _nameDeviceMap[name];
}
