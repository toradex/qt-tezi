#include <QDir>
#include <QDebug>
#include "blockdevinfo.h"
#include "osinfo.h"
#include "partitioninfo.h"
#include "json.h"

OsInfo::OsInfo(const QString &folder, const QString &infofile, QObject *parent) :
    QObject(parent), _folder(folder), _infofile(infofile)
{
    QVariantMap m = Json::loadFromFile(folder + QDir::separator() + infofile).toMap();
    _name = m.value("name").toString();
    _version = m.value("version").toString();
    _description = m.value("description").toString();
    _releaseDate = m.value("release_date").toString();
    _bootable = m.value("bootable", true).toBool();

    QVariantList blockdevs = m.value("blockdevs").toList();
    foreach (QVariant bd, blockdevs)
    {
        QVariantMap blockdev = bd.toMap();

        _blockdevs.append(new BlockDevInfo(blockdev, this));
    }
}
