#include "blockdevinfo.h"
#include "blockdevpartitioninfo.h"

#include <QFile>
#include <QString>

BlockDevInfo::BlockDevInfo(const QVariantMap &blockdev, QObject *parent) :
    QObject(parent), _content(NULL)
{
    _name = BlockDevInfo::getDeviceNameFromSymlink(blockdev.value("name").toString());
    _erase = blockdev.value("erase", false).toBool();
    _tableType = blockdev.value("table_type", "dos").toString();

    /* Partitions within a blockdev */
    if (blockdev.contains("partitions"))
    {
        QVariantList parts = blockdev.value("partitions").toList();
        foreach (QVariant pv, parts)
        {
            _partitions.append(new BlockDevPartitionInfo(pv.toMap(), _tableType, this));
        }
    }

    /* Writing an image directly to blockdev */
    if (blockdev.contains("content")) {
        QVariantMap contentm = blockdev.value("content").toMap();
        _content = new ContentInfo(contentm, this);
    }
}

// returns the real name of a device file, i.e. what
// readlink -e /dev/${device} would return */
QString BlockDevInfo::getDeviceNameFromSymlink(const QString &device)
{
    QString devDir("/dev/");
    QString deviceName(QFile::symLinkTarget(devDir + device.toLatin1()));
    // is it a regular file
    if (deviceName == "")
        return device;
    deviceName.remove(0, devDir.size());
    return deviceName;
}
