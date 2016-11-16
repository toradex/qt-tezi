#include "partitioninfo.h"

PartitionInfo::PartitionInfo(const QVariantMap &m, QObject *parent) :
    QObject(parent)
{
    _wantMaximised = m.value("want_maximised", false).toBool();
    _offset        = m.value("offset_in_sectors").toInt();
    _partitionSizeNominal = m.value("partition_size_nominal").toInt();
    _requiresPartitionNumber = m.value("requires_partition_number").toInt();
    _active        = m.value("active", false).toBool();

    QByteArray defaultPartType;
    if (m.contains("content")) {
        QVariantMap contentm = m.value("content").toMap();
        _content = new BlockDevContentInfo(contentm, this);

        /* Get partiton type from filesystem type of content */
        QByteArray fstype = _content->fsType();
        if (fstype.contains("fat"))
            defaultPartType = "0c"; /* FAT32 LBA */
        else if (fstype == "swap")
            defaultPartType = "82";
        else if (fstype.contains("ntfs"))
            defaultPartType = "07";
        else if (fstype.contains("raw"))
            defaultPartType = "00";
        else
            defaultPartType = "83"; /* Linux native */
    } else {
        defaultPartType = "00";
    }

    _partitionType = m.value("partition_type", defaultPartType).toByteArray();
}

PartitionInfo::PartitionInfo(int partitionNr, int offset, int sectors, const QByteArray &partType, QObject *parent) :
    QObject(parent), _partitionType(partType), _requiresPartitionNumber(partitionNr), _offset(offset), _partitionSizeSectors(sectors), _active(false)
{
}
