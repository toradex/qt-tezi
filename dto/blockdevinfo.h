#ifndef BLOCKDEVINFO_H
#define BLOCKDEVINFO_H

#include <QObject>
#include <QVariantMap>

class BlockDevContentInfo;
class PartitionInfo;

class BlockDevInfo : public QObject
{
    Q_OBJECT
public:
    /* Constructor. Gets called from OsInfo with info from json file */
    explicit BlockDevInfo(const QVariantMap &blockdev, QObject *parent = 0);

    inline void setBlockDevice(const QByteArray &blockdevice)
    {
        _blockDevice = blockdevice;
    }

    inline QByteArray blockDevice()
    {
        return _blockDevice;
    }

    inline QString name()
    {
        return _name;
    }

    inline BlockDevContentInfo *content()
    {
        return _content;
    }

    inline QList<PartitionInfo *> *partitions()
    {
        return &_partitions;
    }

protected:

    QByteArray _blockDevice;
    QList<PartitionInfo *> _partitions;
    QString _name;
    BlockDevContentInfo *_content;
};

#endif // BLOCKDEVINFO_H
