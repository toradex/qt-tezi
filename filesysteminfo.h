#ifndef FILESYSTEMINFO_H
#define FILESYSTEMINFO_H

#include <QObject>
#include <QVariantMap>

/*
 * File System information model
 * Contains information about the content of a block device (e.g. partition)
 */

class FileSystemInfo : public QObject
{
    Q_OBJECT
public:
    explicit FileSystemInfo(const QVariantMap &m, QObject *parent = 0);

    inline bool emptyFS()
    {
        return _emptyFS;
    }

    inline QString filename()
    {
        return _filename;
    }

    inline QByteArray fsType()
    {
        return _fstype;
    }

    inline QByteArray mkfsOptions()
    {
        return _mkfsOptions;
    }

    inline QByteArray ddOptions()
    {
        return _ddOptions;
    }

    inline QByteArray label()
    {
        return _label;
    }

    inline int uncompressedTarballSize()
    {
        return _uncompressedTarballSize;
    }

signals:

public slots:

protected:
    QByteArray _fstype, _mkfsOptions, _ddOptions, _label;
    QString _filename;
    bool _emptyFS;
    int _uncompressedTarballSize;
};

#endif // FILESYSTEMINFO_H
