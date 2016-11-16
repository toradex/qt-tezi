#ifndef BLOCKDEVCONTENTINFO_H
#define BLOCKDEVCONTENTINFO_H

#include <QObject>
#include <QVariantMap>
#include <QStringList>

/*
 * Block device content information
 * Contains information about the content of a block device. This can
 * be a partition (/dev/mmcblk0p0) or a bare block device (/dev/mmcblk0).
 */

class RawFileInfo;

class BlockDevContentInfo : public QObject
{
    Q_OBJECT
public:
    explicit BlockDevContentInfo(const QVariantMap &m, QObject *parent = 0);

    inline QString filename()
    {
        return _filename;
    }

    inline QStringList filelist()
    {
        return _filelist;
    }

    inline QByteArray fsType()
    {
        return _fstype;
    }

    inline QByteArray mkfsOptions()
    {
        return _mkfsOptions;
    }

    inline QByteArray label()
    {
        return _label;
    }

    inline int uncompressedSize()
    {
        return _uncompressedSize;
    }

    inline QList<RawFileInfo *> *rawFiles()
    {
        return &_rawFiles;
    }

signals:

public slots:

protected:
    QByteArray _fstype, _mkfsOptions, _label;
    QString _filename;
    QStringList _filelist;
    QList<RawFileInfo *> _rawFiles;
    int _uncompressedSize;
};

#endif // BLOCKDEVCONTENTINFO_H
