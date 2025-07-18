#include "imagelist.h"
#include "configblock.h"
#include "config.h"

#include <QDir>
#include <QFileInfo>
#include <QVariantMap>
#include <QVersionNumber>
#include <QString>
#include <QDebug>

ImageList::ImageList(const QString& toradexPid8, QObject *parent) : QObject(parent),
    _toradexPid8(toradexPid8)
{

}
// Compare two variants.
bool ImageList::imageSortOrder(const QVariant &v1, const QVariant &v2)
{
    QVariantMap m1 = v1.toMap();
    QVariantMap m2 = v2.toMap();

    if (m1["feedindex"].toInt() == m2["feedindex"].toInt())
        return m1["index"].toInt() < m2["index"].toInt();

    return m1["feedindex"].toInt() < m2["feedindex"].toInt();
}

void ImageList::addImages(QListVariantMap images)
{
    QVariantMap* autoInstallImage = nullptr;

    listMutex.lock();
    for (QVariantMap &m : images)
    {
        int config_format = m.value("config_format").toInt();
        QString foldername = m.value("foldername").toString();
        bool autoInstall = m.value("autoinstall").toBool();
        bool isInstaller = m.value("isinstaller").toBool();
        QString versionString = m.value("version").toString();
        const QStringList supportedProductIds = m.value("supported_product_ids").toStringList();
        bool supportedImage = ConfigBlock::isProductSupported(_toradexPid8, supportedProductIds);
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
            if (isInstaller && autoInstall) {
                bool isNewer = false;
                int installerVersionIndex, imageVersionIndex;
                QString installerVersionString(VERSION_NUMBER);

                QVersionNumber installerVersion = QVersionNumber::fromString(installerVersionString, &installerVersionIndex);
                QVersionNumber imageVersion = QVersionNumber::fromString(versionString, &imageVersionIndex);

                int compVersion = QVersionNumber::compare(installerVersion, imageVersion);
                if (compVersion < 0)
                {
                    // This image is newer...!
                    isNewer = true;
                }
                else if (compVersion > 0)
                {
                    // Current running Tezi is newer...
                    isNewer = false;
                }
                else
                {
                    /*
                     * QVersionNumber only compares the first three numbers (e.g. 5.2.0),
                     * If these version numbers are equal we land in here.
                     *
                     * We can only distinguish between a prerelease (nightly, monthly etc.)
                     * and a release. Check if the Tezi to be installed is a release and the
                     * one that is running is a prerelease, which is the only case it should
                     * autoinstall another Tezi if it has the same version number.
                     */
                    bool installerIsPrerelease, imageIsPrerelease;

                    installerIsPrerelease = installerVersionString.contains("devel");
                    imageIsPrerelease = versionString.contains("devel");

                    if (installerIsPrerelease && !imageIsPrerelease) {
                        // Only a released image vs. a prereleased installer is considered newer
                        isNewer = true;
                    }
                }

                /* Only autoInstall newer Toradex Easy Installer Versions */
                if (!isNewer)
                    autoInstall = false;
            }

            /* We found an auto install image, remember it */
            if (autoInstall)
                autoInstallImage = &m;
        }

        if (source == SOURCE_USB)
            m["url"] = "usb:/" + foldername;
        else if (source == SOURCE_SDCARD)
            m["uri"] = "sdcard:/" + foldername;
        else {
            m["uri"] = m.value("baseurl").value<QString>();
        }

        _imageList.append(m);
    }

    std::sort(_imageList.begin(), _imageList.end(), ImageList::imageSortOrder);
    listMutex.unlock();

    /* If we found an autoinstall image, install it! */
    if (autoInstallImage != nullptr)
        emit installImage(*autoInstallImage, true);

    emit imageListUpdated();
}

void ImageList::addImage(const QVariantMap &image)
{
    QMutexLocker locker(&listMutex);
    _imageList.append(image);
}

void ImageList::removeImagesByBlockdev(const QString blockdev)
{
    QMutexLocker locker(&listMutex);
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
    QMutexLocker locker(&listMutex);
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
