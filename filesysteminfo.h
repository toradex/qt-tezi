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

    inline QByteArray label()
    {
        return _label;
    }

signals:

public slots:

protected:
    QByteArray _fstype, _mkfsOptions, _label;
    QString _filename;
};

#endif // FILESYSTEMINFO_H
