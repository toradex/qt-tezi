#ifndef BLOCKDEVINFO_H
#define BLOCKDEVINFO_H

#include <QObject>
#include <QVariantMap>

class ContentInfo;
class BlockDevPartitionInfo;

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

    inline QString tableType()
    {
        return _tableType;
    }

    inline ContentInfo *content()
    {
        return _content;
    }

    inline QList<BlockDevPartitionInfo *> *partitions()
    {
        return &_partitions;
    }

    inline bool erase()
    {
        return _erase;
    }

protected:

    bool _erase;
    QByteArray _blockDevice;
    QList<BlockDevPartitionInfo *> _partitions;
    QString _name;
    QString _tableType;
    ContentInfo *_content;
};

#endif // BLOCKDEVINFO_H
