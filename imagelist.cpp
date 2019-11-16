#include "imagelist.h"
#include "config.h"

#include <QDir>
#include <QFileInfo>
#include <QVariantMap>
#include <QVariantMap>

ImageList::ImageList(const QString& toradexProductNumber, QObject *parent) : QObject(parent),
    _toradexProductNumber(toradexProductNumber)
{

}

void ImageList::addImages(QListVariantMap images)
{
    QVariantMap* autoInstallImage = nullptr;

    for (QVariantMap &m : images)
    {
        int config_format = m.value("config_format").toInt();
        bool autoInstall = m.value("autoinstall").toBool();
        bool isInstaller = m.value("isinstaller").toBool();
        QString version = m.value("version").toString();
        QVariantList supportedProductIds = m.value("supported_product_ids").toList();
        bool supportedImage = supportedProductIds.contains(_toradexProductNumber);
        bool supportedConfigFormat = config_format <= IMAGE_CONFIG_FORMAT;
        enum ImageSource source = m.value("source").value<enum ImageSource>();
        m["supported_image"] = supportedImage;
        m["supported_config_format"] = supportedConfigFormat;

        if (source == SOURCE_INTERNET && !supportedImage) {
            /* We don't show incompatible images from the Internet (there will be a lot of them later!) */
            removeTemporaryFiles(m);
            continue;
        }

        if (supportedImage && supportedConfigFormat) {
            /* Compare Toradex Easy Installer against current version */
            if (isInstaller) {
                bool isNewer = false;
                QStringList installerversion = QString(VERSION_NUMBER).split('.');
                QStringList imageversion = QString(version).remove(QRegExp("[^0-9|.]")).split('.');

                /* Get minimal version length */
                int versionl = installerversion.length();
                if (versionl < imageversion.length())
                    versionl = imageversion.length();

                /* If we get a longer version, its newer... (0.3 vs. 0.3.1 */
                if (versionl < imageversion.length())
                    isNewer = true;

                for (int i = 0; i < versionl; i++) {
                    if (imageversion[i].toInt() > installerversion[i].toInt())
                        isNewer = true;
                }

                /* Only autoInstall newer Toradex Easy Installer Versions */
                if (!isNewer)
                    autoInstall = false;
            }

            /* We found an auto install image, remember it */
            if (autoInstall)
                autoInstallImage = &m;
        }

        _imageList.append(m);
    }

    /* If we found an autoinstall image, install it! */
    if (autoInstallImage != nullptr)
        emit foundAutoInstallImage(*autoInstallImage);

    emit imageListUpdated();
}

void ImageList::addImage(const QVariantMap &image)
{
    _imageList.append(image);
}

void ImageList::removeImagesByBlockdev(const QString blockdev)
{
    bool changes = false;

    QMutableListIterator<QVariantMap> i(_imageList);
    while (i.hasNext()) {
        if (i.next().value("image_source_blockdev").toString() == blockdev){
            i.remove();
            changes = true;
        }
    }

    if (changes)
        emit imageListUpdated();
}

void ImageList::removeImagesBySource(enum ImageSource source)
{
    bool changes = false;

    QMutableListIterator<QVariantMap> i(_imageList);
    while (i.hasNext()) {
        if (i.next().value("source").value<enum ImageSource>() == source){
            i.remove();
            changes = true;
        }
    }

    if (changes)
        emit imageListUpdated();
}

void ImageList::removeTemporaryFiles(const QVariantMap entry)
{
    QString folder = entry["folder"].toString();
    QDir dir(folder);
    if (dir.exists()) {
        Q_FOREACH(QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files)) {
            QFile::remove(info.absoluteFilePath());
        }
        QDir().rmdir(folder);
    }
}
