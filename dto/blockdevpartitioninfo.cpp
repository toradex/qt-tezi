#include "blockdevpartitioninfo.h"

BlockDevPartitionInfo::BlockDevPartitionInfo(const QVariantMap &m, const QString &tableType, QObject *parent) :
    QObject(parent), _content(NULL)
{
    _wantMaximised = m.value("want_maximised", false).toBool();
    _offset        = m.value("offset_in_sectors").toInt();
    _partitionSizeNominal = m.value("partition_size_nominal").toInt();
    _requiresPartitionNumber = m.value("requires_partition_number").toInt();
    _active        = m.value("active", false).toBool();

    QByteArray defaultPartType;
    if (m.contains("content")) {
        QVariantMap contentm = m.value("content").toMap();
        _content = new ContentInfo(contentm, this);

        /* Get partiton type from filesystem type of content */
        QByteArray fstype = _content->fsType();
        if (tableType == "gpt") {
            if (fstype.contains("fat"))
                defaultPartType = "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7"; /* Microsoft Basic data partition */
            else if (fstype == "swap")
                defaultPartType = "0657FD6D-A4AB-43C4-84E5-0933C84B4F4F";
            else if (fstype.contains("ntfs"))
                defaultPartType = "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7";
            else
                defaultPartType = "0FC63DAF-8483-4772-8E79-3D69D8477DE4"; /* Linux filesystem data */
        } else {
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
        }
    } else {
        if (tableType == "gpt")
            defaultPartType = "0FC63DAF-8483-4772-8E79-3D69D8477DE4"; /* Linux filesystem data */
        else
            defaultPartType = "00";
    }

    _partitionType = m.value("partition_type", defaultPartType).toByteArray();
}

BlockDevPartitionInfo::BlockDevPartitionInfo(int partitionNr, int offset, int sectors, const QByteArray &partType, QObject *parent) :
    QObject(parent), _partitionType(partType), _requiresPartitionNumber(partitionNr), _offset(offset), _partitionSizeSectors(sectors), _active(false)
{
}
