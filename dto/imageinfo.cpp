#include <QDir>
#include <QDebug>
#include "blockdevinfo.h"
#include "mtddevinfo.h"
#include "imageinfo.h"
#include "blockdevpartitioninfo.h"
#include "json.h"

ImageInfo::ImageInfo(const QString &folder, const QString &infofile, const QString &baseUrl, enum ImageSource source, QObject *parent) :
    QObject(parent), _folder(folder), _infofile(infofile), _baseUrl(baseUrl), _imageSource(source)
{
    QVariantMap m = Json::loadFromFile(folder + QDir::separator() + infofile).toMap();
    _name = m.value("name").toString();
    _version = m.value("version").toString();
    _description = m.value("description").toString();
    _releaseDate = m.value("release_date").toString();
    _isInstaller = m.value("isinstaller", false).toBool();
    _prepareScript = m.value("prepare_script").toString();
    _wrapupScript = m.value("wrapup_script").toString();
    _errorScript = m.value("error_script").toString();
    _uBootEnv = m.value("u_boot_env").toString();
    QVariantList productids = m.value("supported_product_ids").toList();
    foreach (QVariant prid, productids) {
        _supportedProductIds.append(prid.toString());
    }

    QVariantList blockdevs = m.value("blockdevs").toList();
    foreach (QVariant bd, blockdevs) {
        QVariantMap blockdev = bd.toMap();

        _blockdevs.append(new BlockDevInfo(blockdev, this));
    }

    QVariantList mtddevs = m.value("mtddevs").toList();
    foreach (QVariant md, mtddevs) {
        QVariantMap mtddev = md.toMap();

        _mtddevs.append(new MtdDevInfo(mtddev, this));
    }
}
