#include "blockdevinfo.h"
#include "partitioninfo.h"

BlockDevInfo::BlockDevInfo(const QVariantMap &blockdev, QObject *parent) :
    QObject(parent)
{
    _name = blockdev.value("name").toString();

    /* Partitions within a blockdev */
    if (blockdev.contains("partitions"))
    {
        QVariantList parts = blockdev.value("partitions").toList();
        foreach (QVariant pv, parts)
        {
            _partitions.append(new PartitionInfo(pv.toMap(), this));
        }
    }

    /* Writing an image directly to blockdev */
    if (blockdev.contains("content")) {
        QVariantMap contentm = blockdev.value("content").toMap();
        _content = new FileSystemInfo(contentm, this);
    }
}
