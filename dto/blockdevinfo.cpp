#include "blockdevinfo.h"
#include "blockdevpartitioninfo.h"

BlockDevInfo::BlockDevInfo(const QVariantMap &blockdev, QObject *parent) :
    QObject(parent), _content(NULL)
{
    _name = blockdev.value("name").toString();
    _erase = blockdev.value("erase", false).toBool();

    /* Partitions within a blockdev */
    if (blockdev.contains("partitions"))
    {
        QVariantList parts = blockdev.value("partitions").toList();
        foreach (QVariant pv, parts)
        {
            _partitions.append(new BlockDevPartitionInfo(pv.toMap(), this));
        }
    }

    /* Writing an image directly to blockdev */
    if (blockdev.contains("content")) {
        QVariantMap contentm = blockdev.value("content").toMap();
        _content = new ContentInfo(contentm, this);
    }
}
