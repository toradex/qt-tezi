#ifndef PARTITIONINFO_H
#define PARTITIONINFO_H

/*
 * Partition information model
 * Contains information about a single partition
 * and runtime information like the partition device (/dev/mmcblk0pX) it was assigned
 */

#include "filesysteminfo.h"
#include <QObject>
#include <QVariantMap>

class PartitionInfo : public QObject
{
    Q_OBJECT
public:
    /* Constructor. Gets called from OsInfo with info from json file */
    explicit PartitionInfo(const QVariantMap &m, QObject *parent = 0);

    explicit PartitionInfo(int partitionNr, int offset, int sectors, const QByteArray &partType, QObject *parent = 0);

    inline void setPartitionDevice(const QByteArray &partdevice)
    {
        _partitionDevice = partdevice;
    }

    inline QByteArray partitionDevice()
    {
        return _partitionDevice;
    }

    inline void setRequiresPartitionNumber(int nr)
    {
        _requiresPartitionNumber = nr;
    }

    inline int requiresPartitionNumber()
    {
        return _requiresPartitionNumber;
    }

    inline FileSystemInfo *content()
    {
        return _content;
    }

    inline int partitionSizeNominal()
    {
        return _partitionSizeNominal;
    }

    inline bool wantMaximised()
    {
        return _wantMaximised;
    }

    inline void setOffset(int offset)
    {
        _offset = offset;
    }

    inline int offset()
    {
        return _offset;
    }

    inline void setPartitionSizeSectors(int size)
    {
        _partitionSizeSectors = size;
    }

    inline int partitionSizeSectors()
    {
        return _partitionSizeSectors;
    }

    inline int endSector()
    {
        return _offset + _partitionSizeSectors;
    }

    inline bool active()
    {
        return _active;
    }

    inline QByteArray partitionType()
    {
        return _partitionType;
    }

protected:
    QByteArray _partitionDevice, _partitionType;
    QString _tarball;
    FileSystemInfo *_content;
    int _partitionSizeNominal, _requiresPartitionNumber, _offset, _partitionSizeSectors;
    bool _wantMaximised, _active;
};

#endif // PARTITIONINFO_H
