#include "partitioninfo.h"

PartitionInfo::PartitionInfo(const QVariantMap &m, QObject *parent) :
    QObject(parent)
{
    _wantMaximised = m.value("want_maximised", false).toBool();
    _emptyFS       = m.value("empty_fs", false).toBool();
    _offset        = m.value("offset_in_sectors").toInt();
    _partitionSizeNominal = m.value("partition_size_nominal").toInt();
    _requiresPartitionNumber = m.value("requires_partition_number").toInt();
    _uncompressedTarballSize = m.value("uncompressed_tarball_size").toInt();
    _active        = m.value("active", false).toBool();

    QVariantMap contentm = m.value("content").toMap();
    _content = new FileSystemInfo(contentm, this);

    /* Get partiton type from filesystem type of content */
    QByteArray defaultPartType;
    QByteArray fstype = _content->fsType();
    if (fstype.contains("fat"))
        defaultPartType = "0c"; /* FAT32 LBA */
    else if (fstype == "swap")
        defaultPartType = "82";
    else if (fstype.contains("ntfs"))
        defaultPartType = "07";
    else
        defaultPartType = "83"; /* Linux native */

    _partitionType = m.value("partition_type", defaultPartType).toByteArray();
}

PartitionInfo::PartitionInfo(int partitionNr, int offset, int sectors, const QByteArray &partType, QObject *parent) :
    QObject(parent), _partitionType(partType), _requiresPartitionNumber(partitionNr), _offset(offset), _partitionSizeSectors(sectors), _active(false)
{
}
