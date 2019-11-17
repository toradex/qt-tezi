#include "mediapollthread.h"
#include "config.h"
#include "util.h"
#include "multiimagewritethread.h"
#include "json.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QIcon>

/*
 * Polls external mass storage media devices such as USB flash drives or
 * SD cards. We use a single mount point for scanning and installation later
 * on. This also makes sure that we are not scanning while installing and
 * visa versa.
 *
 * Initial author: Stefan Agner
 */
MediaPollThread::MediaPollThread(ModuleInformation *moduleInformation, QObject *parent)
    : QThread(parent),
    _moduleInformation(moduleInformation)
{
    QDir dir;
    dir.mkpath(SRC_MOUNT_FOLDER);
    if (isMounted(SRC_MOUNT_FOLDER)) {
        mountMutex.lock();
        unmountMedia();
    }
}

bool MediaPollThread::isMounted(const QString &path) {
    QFile file("/proc/mounts");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&file);
    QString line = in.readLine();
    while (!line.isNull()) {
        if (line.contains(path))
            return true;
        line = in.readLine();
    }

    return false;
}

void MediaPollThread::checkRemovableBlockdev(const QString &nameFilter)
{
    QDir dir("/sys/block/", nameFilter, QDir::Name, QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList list = dir.entryList();

    foreach(QString blockdev, list) {
        QByteArray removable = getFileContents("/sys/block/" + blockdev + "/removable");
        if (!removable.startsWith("1"))
            continue;

        /* Try raw blockdev without any partition table... */
        processMedia(SOURCE_USB, blockdev);

        /* Get partitions of this device */
        QDir devdir("/dev", blockdev + "?*", QDir::Name, QDir::System | QDir::NoDotAndDotDot);
        QStringList devfiles = devdir.entryList();
        foreach (QString devfile, devfiles) {
            processMedia(SOURCE_USB, devfile);
        }
    }
}

void MediaPollThread::checkSDcard()
{
    QDir dir("/sys/block/", "mmcblk*", QDir::Name, QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList list = dir.entryList();


    foreach(QString blockdev, list) {
        /* We need to ignore internal/target block devices on modules with eMMC */
        if (_moduleInformation->storageClass() == ModuleInformation::Block) {
            /*
             * Unfortunately, at least with Linux 4.1, we can't rely on removable property for
             * eMMC/SD card detection... just ignore the block devices we consider as internal...
             */
            if (_moduleInformation->erasePartitions().contains(blockdev))
                continue;
        }

        /* Avoid imx8 I/O hang with rpmb partition on emmcs */
        if (blockdev.contains("rpmb"))
            continue;

        /* Try raw blockdev without any partition table... */
        processMedia(SOURCE_SDCARD, blockdev);

        /* Get partitions of this device */
        QDir devdir("/dev", blockdev + "p?", QDir::Name, QDir::System | QDir::NoDotAndDotDot);
        QStringList devfiles = devdir.entryList();
        foreach (QString devfile, devfiles)
            processMedia(SOURCE_SDCARD, devfile);
    }
}

bool MediaPollThread::mountMedia(const QString &blockdev)
{
    while (!mountMutex.tryLock(2000)) {
        qDebug() << "Trying to mount" << blockdev << ", but something else still mounted...?";

        if (!isMounted(SRC_MOUNT_FOLDER))
            break; // the folder is not mounted anymore, manual user intervene? Just reuse the lock */
    }

    if (QProcess::execute("mount -o ro " + blockdev + " " SRC_MOUNT_FOLDER) != 0)
    {
        mountMutex.unlock();
        qDebug() << "Error mounting external device" << blockdev << "to" << SRC_MOUNT_FOLDER;
        return false;
    }

    return true;
}

bool MediaPollThread::unmountMedia()
{
    if (QProcess::execute("umount " SRC_MOUNT_FOLDER) != 0) {
        qDebug() << "Error unmounting external device" << SRC_MOUNT_FOLDER;
        if (QProcess::execute("umount -f " SRC_MOUNT_FOLDER) != 0) {
            qDebug() << "Error force unmounting external device" << SRC_MOUNT_FOLDER;
            return false;
        }
    }
    qDebug() << "Umounted external device" << SRC_MOUNT_FOLDER;

    mountMutex.unlock();

    return true;
}

void MediaPollThread::processMedia(enum ImageSource src, const QString &blockdev)
{
    QString blockdevpath = "/dev/" + blockdev;

    _blockdevsChecking.insert(blockdevpath);

    /* Did we already checked this block device? */
    if (_blockdevsChecked.contains(blockdevpath))
        return;

    /* Ignore blockdev if it has not a valid file system */
    if (MultiImageWriteThread::getFsType(blockdevpath).length() <= 0)
        return;

    if (!mountMedia(blockdevpath)) {
        emit errorMounting(blockdevpath);
        return;
    }

    qDebug() << "Reading images from media" << blockdevpath;

    // Check for HTTP server urls
    parseTeziConfig(SRC_MOUNT_FOLDER);

    QList<QVariantMap> images = listMediaImages(SRC_MOUNT_FOLDER, blockdevpath, src);
    unmountMedia();
    emit newImagesToAdd(images);
}

QList<QFileInfo> MediaPollThread::findImages(const QString &path, int depth)
{
    QList<QFileInfo> images;

    /* Local image folders, search all subfolders... */
    QDirIterator it(path, QStringList() << "image*.json", QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        it.next();
        QFileInfo fi = it.fileInfo();
        if (fi.isDir() && depth < 3)
            images << findImages(fi.filePath(), depth + 1);
        if (fi.isFile())
            images << fi;
    }

    return images;
}

QList<QVariantMap> MediaPollThread::listMediaImages(const QString &path, const QString &blockdev, enum ImageSource source)
{
    QDir root(path);
    QList<QVariantMap> images;
    QList<QFileInfo> imagefiles = findImages(path, 0);

    foreach(QFileInfo image, imagefiles) {
        QString imagename = root.relativeFilePath(image.path());

        qDebug() << "Adding image" << imagename << "from" << blockdev;

        QVariantMap imagemap = Json::loadFromFile(image.filePath()).toMap();
        imagemap["foldername"] = imagename;
        imagemap["folder"] = image.path();
        imagemap["source"] = source;
        // Use -2 so that local media will be displayed first.
        imagemap["feedindex"] = -2;

        if (!imagemap.contains("nominal_size"))
            imagemap["nominal_size"] = calculateNominalSize(imagemap);

        QString iconFilename = imagemap["icon"].toString();
        if (!iconFilename.isEmpty() && !iconFilename.contains(QDir::separator())) {
            QFile iconfile(image.path() + QDir::separator() + iconFilename);
            iconfile.open(QFile::ReadOnly);
            imagemap["iconimage"] = iconfile.readAll();
        }

        imagemap["image_info"] = image.fileName();
        imagemap["image_source_blockdev"] = blockdev;
        images.append(imagemap);
    }

    return images;
}

/* Calculates nominal image size based on partition information. */
int MediaPollThread::calculateNominalSize(const QVariantMap &imagemap)
{
    int nominal_size = 0;
    QVariantList blockdevs = imagemap.value("blockdevs").toList();
    foreach (QVariant b, blockdevs) {
        QVariantMap blockdev = b.toMap();
        if (blockdev.value("name") == "mmcblk0") {
            QVariantList pvl = blockdev.value("partitions").toList();
            foreach (QVariant v, pvl)
            {
                QVariantMap pv = v.toMap();
                nominal_size += pv.value("partition_size_nominal").toInt();
            }
            break;
        }
    }

    return nominal_size;
}

void MediaPollThread::parseTeziConfig(const QString &path)
{
    QString configjson = path + QDir::separator() + "tezi_config.json";
    if (!QFile::exists(configjson))
        return;

    QVariantMap teziconfig = Json::loadFromFile(configjson).toMap();

    // Enable/Disable default feeds
    if (teziconfig.contains(TEZI_CONFIG_JSON_DEFAULT_FEED)
            && !teziconfig.value(TEZI_CONFIG_JSON_DEFAULT_FEED, true).toBool()) {
        emit disableFeed(TEZI_CONFIG_JSON_DEFAULT_FEED);
        qDebug() << "Default feed disabled";
    }
    if (teziconfig.contains(TEZI_CONFIG_JSON_3RDPARTY_FEED)
            && !teziconfig.value(TEZI_CONFIG_JSON_3RDPARTY_FEED, true).toBool()) {
        emit disableFeed(TEZI_CONFIG_JSON_3RDPARTY_FEED);
        qDebug() << "3rd Party feed disabled";
    }

    if (!teziconfig.contains("image_lists"))
        return;

    QStringList imagelisturls = teziconfig["image_lists"].value<QStringList>();
    qDebug() << "Adding URLs to URL list" << imagelisturls;

    foreach (QString url, imagelisturls)
        emit newImageUrl(url);
}

void MediaPollThread::poll() {
    scanMutex.lock();
    _blockdevsChecking.clear();

    checkRemovableBlockdev("sd*");
    checkSDcard();

    /* Check which blockdevices disappeared since last poll and remove them */
    QSet<QString> lostblockdevs = _blockdevsChecked - _blockdevsChecking;
    foreach(QString blockdev, lostblockdevs) {
        emit removedBlockdev(blockdev);
    }

    _blockdevsChecked = _blockdevsChecking;
    scanMutex.unlock();
}

void MediaPollThread::run() {

    msleep(100);

    qDebug() << "Scanning media initially...";
    poll();
    emit firstScanFinished();

    while (true) {
        sleep(1);
        poll();
    }

    emit finished();
}
