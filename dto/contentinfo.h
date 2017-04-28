#ifndef CONTENTINFO_H
#define CONTENTINFO_H

#include <QObject>
#include <QVariantMap>
#include <QStringList>

/*
 * Content information
 * Contains information about the content of a block device, MTD device or
 * UBI volume. This can be block device partition (/dev/mmcblk0p0) or a
 * bare block device (/dev/mmcblk0), a MTD device (/dev/mtd0) or UBI volume
 * (/dev/ubi0_0).
 */

class RawFileInfo;

class ContentInfo : public QObject
{
    Q_OBJECT
public:
    explicit ContentInfo(const QVariantMap &m, QObject *parent = 0);

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

protected:
    QByteArray _fstype, _mkfsOptions, _label;
    QString _filename;
    QStringList _filelist;
    QList<RawFileInfo *> _rawFiles;
    int _uncompressedSize;
};

#endif // CONTENTINFO_H
