#ifndef OSINFO_H
#define OSINFO_H

/**
 * OS info model
 * Contains the information from os.json, and has a pointer to the partitions
 */

#include <QObject>
#include <QList>

class BlockDevInfo;

class OsInfo : public QObject
{
    Q_OBJECT
public:
    /* Constructor parses the json files in <folder>, and stores information in local variables */
    explicit OsInfo(const QString &folder, const QString &infofile, QObject *parent = 0);

    inline QString folder()
    {
        return _folder;
    }

    inline QString infofile()
    {
        return _infofile;
    }

    inline QString name()
    {
        return _name;
    }

    inline QString description()
    {
        return _description;
    }

    inline QString version()
    {
        return _version;
    }

    inline QString releaseDate()
    {
        return _releaseDate;
    }

    inline bool bootable()
    {
        return _bootable;
    }

    inline QList<BlockDevInfo *> *blockdevs()
    {
        return &_blockdevs;
    }

    inline int riscosOffset()
    {
        return _riscosOffset;
    }

protected:
    QString _folder, _infofile, _name, _description, _version, _releaseDate;
    bool _bootable;
    QList<BlockDevInfo *> _blockdevs;
    int _riscosOffset;

};

#endif // OSINFO_H
