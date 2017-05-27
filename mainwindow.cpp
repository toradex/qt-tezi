#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "multiimagewritethread.h"
#include "confeditdialog.h"
#include "progressslideshowdialog.h"
#include "config.h"
#include "configblockdialog.h"
#include "resourcedownload.h"
#include "languagedialog.h"
#include "scrolltextdialog.h"
#include "json.h"
#include "util.h"
#include "twoiconsdelegate.h"
#include "wifisettingsdialog.h"
#include "discardthread.h"
#include "mtderasethread.h"
#include <QMessageBox>
#include <QProgressDialog>
#include <QMap>
#include <QProcess>
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <QKeyEvent>
#include <QApplication>
#include <QScreen>
#include <QSplashScreen>
#include <QDesktopWidget>
#include <QSettings>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkDiskCache>
#include <QtNetwork/QNetworkInterface>
#include <QtNetwork/QNetworkConfigurationManager>
#include <QtDBus/QDBusConnection>
#include <QHostInfo>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <sys/time.h>

#ifdef Q_WS_QWS
#include <QWSServer>
#endif

/* Main window
 *
 * Initial author: Floris Bos
 * Maintained by Raspberry Pi
 *
 * See LICENSE.txt for license details
 *
 */

MainWindow::MainWindow(QSplashScreen *splash, LanguageDialog* ld, bool allowAutoinstall, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    _qpd(NULL), _allowAutoinstall(allowAutoinstall), _isAutoinstall(false), _showAll(false), _newInstallerAvailable(false),
    _splash(splash), _ld(ld), _wasOnline(false), _wasRndis(false), _netaccess(NULL), _numDownloads(0), _mediaMounted(false),
    _firstMediaPoll(true)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowState(Qt::WindowMaximized);
    setContextMenuPolicy(Qt::NoContextMenu);
    ui->setupUi(this);

    _moduleInformation = ModuleInformation::detectModule();
    if (_moduleInformation == NULL) {
        QMessageBox::critical(NULL, QObject::tr("Module Detection failed"),
                              QObject::tr("Failed to detect the basic module type. Cannot continue."),
                              QMessageBox::Close);
        QApplication::exit(LINUX_POWEROFF);
    }

    _toradexConfigBlock = _moduleInformation->readConfigBlock();

    // No config block found, ask the user to create a new one using Label information
    if (_toradexConfigBlock == NULL) {
        QMessageBox::critical(NULL, QObject::tr("Reading Config Block failed"),
                              QObject::tr("Reading the Toradex Config Block failed, the Toradex Config Block might be erased or corrupted. Please restore the Config Block using the information on the Modules Sticker before continuing."),
                              QMessageBox::Ok);

        ConfigBlockDialog* cbd = new ConfigBlockDialog(_moduleInformation->productIds(), this);
        if (cbd->exec() == QDialog::Accepted) {
            // The user created a new config block, it will be written to NAND once we flash an image...
            _toradexConfigBlock = cbd->configBlock;
        } else {
            QApplication::exit(LINUX_POWEROFF);
        }
    }

    updateVersion();
    _toradexProductName = _toradexConfigBlock->getProductName();
    _toradexBoardRev = _toradexConfigBlock->getBoardRev();
    _serialNumber = _toradexConfigBlock->getSerialNumber();
    _toradexProductId = _toradexConfigBlock->getProductId();
    _toradexProductNumber = _toradexConfigBlock->getProductNumber();
    updateModuleInformation();

    ui->list->setItemDelegate(new TwoIconsDelegate(this));
    ui->list->setIconSize(QSize(40, 40));
    ui->advToolBar->setVisible(false);

    QString cmdline = getFileContents("/proc/cmdline");
    if (cmdline.contains("showall"))
    {
        _showAll = true;
    }

    /* Initialize icons */
    _sdIcon = QIcon(":/icons/sd_memory.png");
    _usbIcon = QIcon(":/icons/flashdisk_logo.png");
    _internetIcon = QIcon(":/icons/download_cloud.png");

    QDir dir;
    dir.mkpath(SRC_MOUNT_FOLDER);

    if (isMounted(SRC_MOUNT_FOLDER))
        unmountMedia();

    ui->list->setSelectionMode(QAbstractItemView::SingleSelection);

    QString sysfsSize = QString("/sys/class/%1/%2/size").arg(_targetDeviceClass, _targetDevice);
    _availableMB = getFileContents(sysfsSize).trimmed().toULongLong();
    if (_targetDeviceClass == "block")
        _availableMB /= 2048;
    else if (_targetDeviceClass == "mtd")
        _availableMB /= (1024 * 1024);

    _usbGadget = new UsbGadget(_serialNumber, _toradexProductName, _toradexProductId);
    if (_usbGadget->initMassStorage())
        ui->actionUsbMassStorage->setEnabled(true);
    else
        ui->actionUsbMassStorage->setEnabled(false);

    if (_usbGadget->initRndis()) {
        ui->actionUsbRndis->setEnabled(true);
        ui->actionUsbRndis->trigger();

    } else {
        ui->actionUsbRndis->setEnabled(false);
    }

    // Add static server list
    _networkUrlList.append(QString(DEFAULT_IMAGE_SERVER).split(' ', QString::SkipEmptyParts));

    connect(&_mediaPollTimer, SIGNAL(timeout()), SLOT(pollMedia()));
    _mediaPollTimer.start(100);
}

MainWindow::~MainWindow()
{
    QProcess::execute("umount " SRC_MOUNT_FOLDER);
    delete ui;
}

void MainWindow::updateModuleInformation()
{
    ui->moduleType->setText(_toradexProductName);
    ui->moduleVersion->setText(_toradexBoardRev);
    ui->moduleSerial->setText(_serialNumber);
}

/* Discover which images we have, and fill in the list */
void MainWindow::show()
{
    QWidget::show();
    showProgressDialog(tr("Wait for external media or network..."));
}

void MainWindow::showProgressDialog(const QString &labelText)
{
    /* Ask user to wait while list is populated */
    _qpd = new QProgressDialog(labelText, QString(), 0, 0, this);
    _qpd->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    _qpd->setModal(true);
    _qpd->show();
}

void MainWindow::addImages(QList<QVariantMap> images)
{
    QSize currentsize = ui->list->iconSize();
    int validImages = 0;
    bool isAutoinstall = false;
    QVariantMap autoInstallImage;

    foreach (QVariantMap m, images)
    {
        int config_format = m.value("config_format").toInt();
        QString name = m.value("name").toString();
        QString foldername = m.value("foldername").toString();
        QString description = m.value("description").toString();
        QString version = m.value("version").toString();
        QString releasedate = m.value("release_date").toString();
        QIcon icon = m.value("iconimage").value<QIcon>();
        bool autoInstall = m.value("autoinstall").toBool();
        bool isInstaller = m.value("isinstaller").toBool();
        QVariantList supportedProductIds = m.value("supported_product_ids").toList();
        bool supportedConfigFormat = config_format <= IMAGE_CONFIG_FORMAT;
        bool supportedImage = supportedProductIds.contains(_toradexProductNumber);
        enum ImageSource source = (enum ImageSource)m.value("source").value<int>();

        if (source == SOURCE_NETWORK) {
            /* We don't show incompatible images from network (there will be a lot of them later!) */
            if (!supportedImage) {
                removeTemporaryFiles(m);
                continue;
            }
        }

        if (supportedImage && supportedConfigFormat) {
            /* Compare Toradex Easy Installer against current version */
            if (isInstaller) {
                bool isNewer = false;
                QStringList installerversion = QString(VERSION_NUMBER).split('.');
                QStringList imageversion = version.remove(QRegExp("[^0-9|.]")).split('.');

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

                if (isNewer)
                    _newInstallerAvailable = true;

                /* Only autoInstall newer Toradex Easy Installer Versions */
                if (!isNewer)
                    autoInstall = false;
            }

            /* We found an auto install image, install it! */
            if (_allowAutoinstall && autoInstall) {
                isAutoinstall = true;
                autoInstallImage = m;
            }
        }

        QString friendlyname = name + "\n";
        if (!version.isEmpty())
            friendlyname += version;
        else
            friendlyname += "Unknown Version";
        if (!releasedate.isEmpty())
            friendlyname += ", " + releasedate;

        if (source == SOURCE_USB)
            friendlyname += ", usb:/" + foldername;
        else if (source == SOURCE_SDCARD)
            friendlyname += ", sdcard:/" + foldername;
        else {
            QString url = m.value("baseurl").value<QString>();
            friendlyname += ", " + url;
        }

        if (icon.isNull()) {
            icon = QIcon();
        } else {
            QList<QSize> avs = icon.availableSizes();
            if (avs.isEmpty()) {
                /* Icon file corrupt */
                icon = QIcon();
            } else {
                QSize iconsize = avs.first();

                if (iconsize.width() > currentsize.width() || iconsize.height() > currentsize.height())
                {
                    /* Make all icons as large as the largest icon we have */
                    currentsize = QSize(qMax(iconsize.width(), currentsize.width()),qMax(iconsize.height(), currentsize.height()));
                    ui->list->setIconSize(currentsize);
                }
            }
        }

        if (!supportedImage)
            friendlyname += "\n[" + tr("This image is not compatible with the current module") + "]";
        else if (!supportedConfigFormat)
            friendlyname += "\n[" + tr("This image requires a newer version of the Toradex Easy Installer") + "]";

        QListWidgetItem *item = new QListWidgetItem(icon, friendlyname);

        item->setData(Qt::UserRole, m);
        item->setToolTip(description);

        if (supportedImage && supportedConfigFormat) {
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            validImages++;
        } else {
            item->setFlags(Qt::NoItemFlags);
        }

        if (source == SOURCE_USB)
            item->setData(SecondIconRole, _usbIcon);
        else if (source == SOURCE_SDCARD)
            item->setData(SecondIconRole, _sdIcon);
        else if (ImageInfo::isNetwork(source))
            item->setData(SecondIconRole, _internetIcon);

        // Limit width of item so that we always see the right icon and don't get a horizontal scrollbar
        item->setSizeHint(QSize(ui->list->width() - 24, ui->list->iconSize().height()));
        ui->list->addItem(item);
    }

    /* Giving items without icon a dummy icon to make them have equal height and text alignment */
    QPixmap dummyicon = QPixmap(currentsize.width(), currentsize.height());
    dummyicon.fill();

    for (int i=0; i< ui->list->count(); i++)
    {
        if (ui->list->item(i)->icon().isNull())
        {
            ui->list->item(i)->setIcon(dummyicon);
        }
    }

    if (validImages > 0)
        ui->actionCancel->setEnabled(true);

    /* Hide progress dialog since we have images we could use now... */
    if (_qpd) {
        if (validImages > 0) {
            _qpd->hide();
            _qpd->deleteLater();
            _qpd = NULL;
        } else {
            _qpd->setLabelText(tr("Wait for external media or network..."));
        }
    }

    if (isAutoinstall) {
        _isAutoinstall = isAutoinstall;
        installImage(autoInstallImage);
        return;
    }

    updateNeeded();
}

void MainWindow::removeTemporaryFiles(const QVariantMap entry)
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

void MainWindow::removeImagesByBlockdev(const QString &blockdev)
{
    for (int i=0; i< ui->list->count(); i++)
    {
        QListWidgetItem *item = ui->list->item(i);
        QVariantMap entry = item->data(Qt::UserRole).toMap();
        if (entry.value("image_source_blockdev").toString() == blockdev) {
            delete item;
            i--;
        }
    }
}

void MainWindow::removeImagesBySource(enum ImageSource source)
{
    for (int i=0; i< ui->list->count(); i++)
    {
        QListWidgetItem *item = ui->list->item(i);
        QVariantMap entry = item->data(Qt::UserRole).toMap();
        QVariant entrysource = entry["source"];
        if (entrysource == source) {
            if (entrysource == SOURCE_NETWORK || entrysource == SOURCE_RNDIS)
                removeTemporaryFiles(entry);
            delete item;
            i--;
        }
    }
}

bool MainWindow::isMounted(const QString &path) {
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

void MainWindow::checkRemovableBlockdev(const QString &nameFilter)
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

void MainWindow::checkSDcard()
{
    QDir dir("/sys/block/", "mmcblk*", QDir::Name, QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList list = dir.entryList();

    // Unfortunately we can't rely on removable property for SD card... just ignore mmcblk0 and take everything else
    foreach(QString blockdev, list) {
        if (blockdev.startsWith(_targetDevice))
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

bool MainWindow::mountMedia(const QString &blockdev)
{
    if (_mediaMounted ||
        QProcess::execute("mount " + blockdev + " " SRC_MOUNT_FOLDER) != 0)
    {
        qDebug() << "Error mounting external device" << blockdev << "to" << SRC_MOUNT_FOLDER;
        QMessageBox::critical(this, tr("Error mounting"),
                              tr("Error mounting external device (%1)").arg(blockdev),
                              QMessageBox::Close);
        return false;
    }

    _mediaMounted = true;

    return true;
}

bool MainWindow::unmountMedia()
{
    if (QProcess::execute("umount " SRC_MOUNT_FOLDER) != 0) {
        qDebug() << "Error unmounting external device" << SRC_MOUNT_FOLDER;
        return false;
    }

    _mediaMounted = false;

    return true;
}

void MainWindow::processMedia(enum ImageSource src, const QString &blockdev)
{
    QString blockdevpath = "/dev/" + blockdev;

    _blockdevsChecking.insert(blockdevpath);

    /* Did we already checked this block device? */
    if (_blockdevsChecked.contains(blockdevpath))
        return;

    /* Ignore blockdev if it has not a valid file system */
    if (MultiImageWriteThread::getFsType(blockdevpath).length() <= 0)
        return;

    qDebug() << "Reading images from media" << blockdevpath;
    if (_qpd) {
        _qpd->setLabelText(tr("Reading images from device %1...").arg(blockdevpath));
        QApplication::processEvents();
    }

    if (mountMedia(blockdevpath))
    {
        // Check for HTTP server urls
        parseTeziConfig(SRC_MOUNT_FOLDER);

        QList<QVariantMap> images = listMediaImages(SRC_MOUNT_FOLDER, blockdevpath, src);
        unmountMedia();
        addImages(images);
    }
}

void MainWindow::pollMedia()
{
    _blockdevsChecking.clear();

    checkRemovableBlockdev("sd*");
    checkSDcard();

    /* Check which blockdevices disappeared since last poll and remove them */
    QSet<QString> lostblockdevs = _blockdevsChecked - _blockdevsChecking;
    foreach(QString blockdev, lostblockdevs) {
        removeImagesByBlockdev(blockdev);
    }

    _blockdevsChecked = _blockdevsChecking;

    /* After first poll, set intervall to 1s and start networking... */
    if (_firstMediaPoll) {
        _mediaPollTimer.setInterval(1000);
        startNetworking();
        _firstMediaPoll = false;
    }
}

void MainWindow::parseTeziConfig(const QString &path)
{
    QString configjson = path + QDir::separator() + "tezi_config.json";
    if (!QFile::exists(configjson))
        return;

    QVariantMap teziconfig = Json::loadFromFile(configjson).toMap();
    QStringList imagelisturls = teziconfig["image_lists"].value<QStringList>();
    qDebug() << "Adding URLs to URL list" << imagelisturls;

    foreach (QString url, imagelisturls) {
        if (url.contains(RNDIS_ADDRESS))
            _rndisUrlList.append(url);
        else
            _networkUrlList.append(url);
    }
}

QList<QVariantMap> MainWindow::listMediaImages(const QString &path, const QString &blockdev, enum ImageSource source)
{
    QList<QVariantMap> images;

    /* Local image folders, search all subfolders... */
    QDir dir(path, "", QDir::Name, QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList list = dir.entryList();

    /* ...and root too */
    list.append("");

    foreach(QString image, list) {
        QString imagefolder = path + QDir::separator() + image;
        QString imagejson = imagefolder + QDir::separator() + "image.json";
        if (!QFile::exists(imagejson))
            continue;

        qDebug() << "Adding image" << image << "from" << blockdev;

        QVariantMap imagemap = Json::loadFromFile(imagejson).toMap();
        imagemap["foldername"] = image;
        imagemap["folder"] = imagefolder;
        imagemap["source"] = source;

        if (!imagemap.contains("nominal_size"))
            imagemap["nominal_size"] = calculateNominalSize(imagemap);

        QString iconFilename = imagemap["icon"].toString();
        if (!iconFilename.isEmpty() && !iconFilename.contains(QDir::separator())) {
            QPixmap pix;
            pix.load(imagefolder + QDir::separator() + iconFilename);
            QIcon icon(pix);
            imagemap["iconimage"] = icon;
        }

        imagemap["image_info"] = "image.json";
        imagemap["image_source_blockdev"] = blockdev;
        images.append(imagemap);
    }

    return images;
}

/* Calculates nominal image size based on partition information. */
int MainWindow::calculateNominalSize(const QVariantMap &imagemap)
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

void MainWindow::on_list_currentItemChanged()
{
    updateNeeded();
}

void MainWindow::on_list_itemDoubleClicked()
{
    on_actionInstall_triggered();
}

void MainWindow::installImage(QVariantMap entry)
{
    enum ImageSource imageSource = (enum ImageSource)entry.value("source").value<int>();

    setEnabled(false);
    _numMetaFilesToDownload = 0;

    /* Stop any polling, we are about to install a image */
    _networkStatusPollTimer.stop();
    _mediaPollTimer.stop();
    emit abortAllDownloads();

    if (ImageInfo::isNetwork(imageSource))
    {
        _imageEntry = entry;

        QString folder = entry.value("folder").value<QString>();
        QString url = entry.value("baseurl").value<QString>();

        if (entry.contains("marketing"))
            downloadMetaFile(url + entry.value("marketing").toString(), folder+"/marketing.tar");

        if (entry.contains("prepare_script")) {
            QString script = entry.value("prepare_script").toString();
            downloadMetaFile(url + script, folder + "/" + script);
        }
        if (entry.contains("wrapup_script")) {
            QString script = entry.value("wrapup_script").toString();
            downloadMetaFile(url + script, folder + "/" + script);
        }
        if (entry.contains("license")) {
            QString script = entry.value("license").toString();
            downloadMetaFile(url + script, folder + "/" + script);
        }
        if (entry.contains("releasenotes")) {
            QString script = entry.value("releasenotes").toString();
            downloadMetaFile(url + script, folder + "/" + script);
        }
    }

    if (_numMetaFilesToDownload == 0)
    {
        /* All OSes selected are local */
        startImageWrite(entry);
    }
    else
    {
        _qpd = new QProgressDialog(tr("The install process will begin shortly."), QString(), 0, 0, this);
        _qpd->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
        _qpd->show();
    }
}

void MainWindow::on_actionUsbMassStorage_triggered(bool checked)
{
    if (!checked && !_usbGadget->isMassStorageSafeToRemove()) {
        if (QMessageBox::warning(this,
                                tr("Warning"),
                                tr("Warning: The mass storage has not been properly removed on the USB host side. Remove USB flash drive anyway?"),
                                QMessageBox::Yes, QMessageBox::No) == QMessageBox::No) {
            ui->actionUsbMassStorage->setChecked(true);
            return;
        }
    }

    _usbGadget->enableMassStorage(checked);

    /* Disable installation button if USB mass storage is exported */
    ui->actionInstall->setEnabled(!checked);
    ui->actionEraseModule->setEnabled(!checked);
    ui->actionUsbRndis->setEnabled(!checked);
}

void MainWindow::on_actionUsbRndis_triggered(bool checked)
{
    _usbGadget->enableRndis(checked);
    ui->actionUsbMassStorage->setEnabled(!checked);
}

void MainWindow::on_actionEraseModule_triggered()
{
    if (_targetDeviceClass == "block")
        discardBlockdev();
    else if (_targetDeviceClass == "mtd")
        eraseMtd();
}

void MainWindow::eraseMtd()
{
    if (QMessageBox::warning(this,
                            tr("Confirm"),
                            tr("This erases all data on the internal raw NAND flash, including boot loader and boot loader configuration as well as block erase counters. "
                               "After this operation you either need to install an image or use the modules recovery mode to boot back into Toradex Easy Installer. Continue?"),
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
    {
        showProgressDialog(tr("Erasing all data on internal raw NAND..."));

        /*
         * Preserve first block, in Tezi due to mtd0 being the full NAND flash,
         * this is MTD partition 1 (e.g. mx7-bcb)
         */
        QFile mtdDev("/dev/mtd1");
        mtdDev.open(QFile::ReadOnly);
        _nandBootBlock = new QByteArray(mtdDev.readAll());
        mtdDev.close();

        QStringList mtdDevs;
        mtdDevs << "/dev/mtd0";
        MtdEraseThread *thread = new MtdEraseThread(mtdDevs);

        connect(thread, SIGNAL (finished()), this, SLOT (eraseFinished()));
        connect(thread, SIGNAL (error(QString)), this, SLOT (discardOrEraseError(QString)));

        thread->start();
    }
}

void MainWindow::eraseFinished()
{
    DiscardThread *thread = qobject_cast<DiscardThread *>(sender());

    /* Restore first block... */
    QFile mtdDev("/dev/mtd1");
    mtdDev.open(QFile::WriteOnly);
    mtdDev.write(*_nandBootBlock);
    mtdDev.close();
    delete _nandBootBlock;

    if (_qpd)
        _qpd->hide();

    thread->deleteLater();
}

void MainWindow::discardBlockdev()
{
    if (QMessageBox::warning(this,
                            tr("Confirm"),
                            tr("This discards all data on the internal eMMC, including boot loader and boot loader configuration. "
                               "After this operation you either need to install an image or use the modules recovery mode to boot back into Toradex Easy Installer. Continue?"),
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
    {
        showProgressDialog(tr("Discarding all data on internal eMCC..."));
        QStringList blockDevs;
        blockDevs << "/dev/mmcblk0" << "/dev/mmcblk0boot0" << "/dev/mmcblk0boot1";
        DiscardThread *thread = new DiscardThread(blockDevs);

        connect(thread, SIGNAL (finished()), this, SLOT (discardFinished()));
        connect(thread, SIGNAL (error(QString)), this, SLOT (discardOrEraseError(QString)));

        thread->start();
    }
}

void MainWindow::discardFinished()
{
    DiscardThread *thread = qobject_cast<DiscardThread *>(sender());

    /* Restore config block... */
    if (_toradexConfigBlock)
        _toradexConfigBlock->writeToBlockdev(_moduleInformation->configBlockPartition(), _moduleInformation->configBlockOffset());

    if (_qpd)
        _qpd->hide();

    thread->deleteLater();
}

void MainWindow::discardOrEraseError(const QString &errorString)
{
    QThread *thread = qobject_cast<QThread *>(sender());

    if (_qpd)
        _qpd->hide();

    QMessageBox::critical(this, tr("Error"), errorString, QMessageBox::Close);

    thread->deleteLater();
}

void MainWindow::on_actionShowLicense_triggered()
{
    QFile licensefile(":/LICENSE.txt");
    licensefile.open(QIODevice::ReadOnly);
    ScrollTextDialog license(tr("License"), QString(licensefile.readAll()), QDialogButtonBox::Ok);
    license.setDefaultButton(QDialogButtonBox::Ok);
    license.exec();
}

void MainWindow::on_actionInstall_triggered()
{
    if (!ui->list->currentItem())
        return;

    if (QMessageBox::warning(this,
                            tr("Confirm"),
                            tr("Warning: this will install the selected Image. All existing data on the internal flash will be overwritten."),
                            QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
    {
        QListWidgetItem *item = ui->list->currentItem();
        QVariantMap entry = item->data(Qt::UserRole).toMap();
        installImage(entry);
    }
}

void MainWindow::on_actionRefreshCloud_triggered()
{
    bool downloading = false;

    showProgressDialog("");
    removeImagesBySource(SOURCE_NETWORK);
    if (hasAddress("eth0")) {
        if (!_networkUrlList.empty()) {
            downloadLists(_networkUrlList);
            downloading = true;
        }
    }

    removeImagesBySource(SOURCE_RNDIS);
    if (hasAddress("usb0")) {
        if (!_rndisUrlList.empty()) {
            downloadLists(_rndisUrlList);
            downloading = true;
        }
    }

    // Hide dialog
    if (_qpd && !downloading)
    {
        _qpd->hide();
        _qpd->deleteLater();
        _qpd = NULL;
    }
}

void MainWindow::on_actionCancel_triggered()
{
    close();
}

void MainWindow::onCompleted()
{
    _psd->close();
    _psd->deleteLater();
    _psd = NULL;

    if (_mediaMounted)
        unmountMedia();

    /* Directly reboot into newer Toradex Easy Installer */
    if (_imageWriteThread->getImageInfo()->isInstaller() && _isAutoinstall) {
        close();
        /* A case for kexec... Anyone? :-) */
        QApplication::exit(LINUX_REBOOT);
    }

    QMessageBox msgbox(QMessageBox::Information,
                       tr("Image Installed"),
                       tr("The Image has been installed successfully.") + " <b>" + tr("You can now safely power off or reset the system.") + "</b><br><br>" +
                       tr("In case recovery mode has been used a power cycle will be necessary."),
                       QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, this);
    msgbox.button(QMessageBox::Yes)->setText(tr("Power off"));
    msgbox.button(QMessageBox::No)->setText(tr("Reboot"));
    msgbox.button(QMessageBox::Cancel)->setText(tr("Return to menu"));

    int value = msgbox.exec();
    _imageWriteThread->deleteLater();

    switch (value) {
    case QMessageBox::Yes:
        close();
        QApplication::exit(LINUX_POWEROFF);
        break;
    case QMessageBox::No:
        close();
        QApplication::exit(LINUX_REBOOT);
        break;
    case QMessageBox::Cancel:
        // Return to main menu...
        reenableImageChoice();
        QWidget::show();
        if (_ld != NULL)
            _ld->show();
        break;
    }
}

void MainWindow::onError(const QString &msg)
{
    qDebug() << "Error:" << msg;

    if (_mediaMounted)
        unmountMedia();

    QMessageBox::critical(this, tr("Error"),
                          msg  + "\n\n" +
                          tr("The image has not been written completely. Please restart the process, otherwise you might end up in a non-bootable system."),
                          QMessageBox::Ok);

    _psd->close();
    _psd->deleteLater();
    _psd = NULL;

    _imageWriteThread->deleteLater();
    reenableImageChoice();
    QWidget::show();
    if (_ld != NULL)
        _ld->show();
}

void MainWindow::onQuery(const QString &msg, const QString &title, QMessageBox::StandardButton* answer)
{
    *answer = QMessageBox::question(this, title, msg, QMessageBox::Yes|QMessageBox::No);
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event && event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        updateModuleInformation();
        updateNeeded();
        updateVersion();
        //repopulate();
    }

    QMainWindow::changeEvent(event);
}

void MainWindow::updateVersion()
{
    QString version = getVersionString();
    setWindowTitle(version);
    ui->version->setText(version);
}

void MainWindow::on_actionAdvanced_triggered(bool checked)
{
    ui->advToolBar->setVisible(checked);
}

void MainWindow::on_actionEdit_config_triggered()
{
}

void MainWindow::startNetworking()
{
    _time.start();

    /* We could ask Qt's Bearer management to notify us once we are online,
       but it tends to poll every 10 seconds.
       Users are not that patient, so lets poll ourselves every 0.1 second */
    //QNetworkConfigurationManager *_netconfig = new QNetworkConfigurationManager(this);
    //connect(_netconfig, SIGNAL(onlineStateChanged(bool)), this, SLOT(onOnlineStateChanged(bool)));
    connect(&_networkStatusPollTimer, SIGNAL(timeout()), SLOT(pollNetworkStatus()));
    _networkStatusPollTimer.start(100);
}

bool MainWindow::hasAddress(const QString &iface, QNetworkAddressEntry *currAddress)
{
    /* Check if we have an IP-address other than localhost */
    QList<QNetworkAddressEntry> addresses = QNetworkInterface::interfaceFromName(iface).addressEntries();

    foreach (QNetworkAddressEntry ae, addresses)
    {
        QHostAddress a = ae.ip();
        if (a != QHostAddress::LocalHost && a != QHostAddress::LocalHostIPv6 &&
            a.scopeId() == "") {
            if (currAddress != NULL)
                *currAddress = ae;
            return true;
        }
    }

    return false;
}

void MainWindow::pollNetworkStatus()
{
    QNetworkAddressEntry ae;
    if (hasAddress("eth0", &ae)) {
        if (!_wasOnline) {
            qDebug() << "Network up in" << _time.elapsed()/1000.0 << "seconds";
            ui->labelEthernetAddress->setText(
                        QString("%2/%3").arg(ae.ip().toString(), QString::number(ae.prefixLength())));

            if (_qpd)
                _qpd->setLabelText(tr("Downloading image list ..."));

            downloadLists(_networkUrlList);
            _wasOnline = true;
        }
    } else {
        if (_wasOnline) {
            ui->labelEthernetAddress->setText(tr("No address assigned"));
            removeImagesBySource(SOURCE_NETWORK);
            _wasOnline = false;
        }
    }

    if (hasAddress("usb0", &ae)) {

        if (!_wasRndis) {
            qDebug() << "RNDIS up in" << _time.elapsed()/1000.0 << "seconds";
            ui->labelRNDISAddress->setText(
                        QString("%2/%3").arg(ae.ip().toString(), QString::number(ae.prefixLength())));

            downloadLists(_rndisUrlList);
            _wasRndis = true;
        }
    } else {
        if (_wasRndis) {
            ui->labelRNDISAddress->setText(tr("No address assigned"));
            removeImagesBySource(SOURCE_RNDIS);
            _wasRndis = false;
        }
    }
}

void MainWindow::downloadLists(const QStringList &urls)
{
    if (!_netaccess)
    {
        _netaccess = new QNetworkAccessManager(this);
        QNetworkDiskCache *_cache = new QNetworkDiskCache(this);
        _cache->setCacheDirectory("/tmp/");
        _cache->setMaximumCacheSize(8 * 1024 * 1024);
        _netaccess->setCache(_cache);
        QNetworkConfigurationManager manager;
        _netaccess->setConfiguration(manager.defaultConfiguration());
    }

    foreach (QString url, urls)
    {
        _numDownloads++;
        ResourceDownload *rd = new ResourceDownload(_netaccess, url, NULL);
        connect(rd, SIGNAL(failed()), this, SLOT(downloadListJsonFailed()));
        connect(rd, SIGNAL(completed()), this, SLOT(downloadListJsonCompleted()));
        connect(rd, SIGNAL(finished()), this, SLOT(downloadFinished()));
        connect(this, SIGNAL(abortAllDownloads()), rd, SLOT(abortDownload()));
    }
}

void MainWindow::downloadListJsonCompleted()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());
    int index = 0;

    QVariant json = Json::parse(rd->data());
    rd->deleteLater();
    if (json.isNull()) {
        QMessageBox::critical(this, tr("Error"), tr("Error parsing list JSON downloaded from server"), QMessageBox::Close);
        return;
    }

    QVariantMap map = json.toMap();
    if (map.value("config_format").toInt() > IMAGE_LIST_CONFIG_FORMAT) {
        QMessageBox::critical(this, tr("Error"), tr("Image list config format not supported!"), QMessageBox::Close);
        return;
    }

    QVariantList list = map.value("images").toList();
    QString sourceurlpath = getUrlPath(rd->urlString());

    foreach (QVariant image, list)
    {
        QString url = image.toString();

        // Relative URL...
        if (!url.startsWith("http"))
            url = sourceurlpath + url;

        _numDownloads++;
        ResourceDownload *rd = new ResourceDownload(_netaccess, url, NULL, index);
        connect(rd, SIGNAL(failed()), this, SLOT(downloadImageJsonFailed()));
        connect(rd, SIGNAL(completed()), this, SLOT(downloadImageJsonCompleted()));
        connect(rd, SIGNAL(finished()), this, SLOT(downloadFinished()));
        connect(this, SIGNAL(abortAllDownloads()), rd, SLOT(abortDownload()));
        index++;
    }
}


void MainWindow::downloadListJsonFailed()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    if (rd->networkError() != QNetworkReply::NoError) {
        QMessageBox::critical(this, tr("Download error"), tr("Error downloading image list: %1\nURL: %2").arg(rd->networkErrorString(), rd->urlString()), QMessageBox::Close);
    } else {
        QMessageBox::critical(this, tr("Download error"), tr("Error downloading image list: HTTP status code %1\nURL: %2").arg(rd->httpStatusCode() + "", rd->urlString()), QMessageBox::Close);
    }
}

void MainWindow::downloadImageJsonCompleted()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    QByteArray json = rd->data();
    QVariantMap imagemap = Json::parse(json).toMap();

    QString baseurl = getUrlPath(rd->urlString());
    QString basename = getUrlTopDir(baseurl);
    QString folder = "/var/volatile/" + basename;
    QDir d;
    while (d.exists(folder))
        folder += '_';
    d.mkpath(folder);
    imagemap["folder"] = folder;
    imagemap["index"] = rd->index();

    if (!imagemap.contains("nominal_size"))
        imagemap["nominal_size"] = calculateNominalSize(imagemap);

    QFile imageinfo(folder + "/image.json");
    imageinfo.open(QIODevice::WriteOnly | QIODevice::Text);
    imageinfo.write(json);
    imageinfo.close();
    imagemap["image_info"] = "image.json";
    imagemap["baseurl"] = baseurl;
    if (baseurl.contains(RNDIS_ADDRESS))
        imagemap["source"] = SOURCE_RNDIS;
    else
        imagemap["source"] = SOURCE_NETWORK;
    _netImages.append(imagemap);

    QString icon = imagemap.value("icon").toString();
    if (!icon.isNull()) {
        QString iconurl = icon;
        if (!icon.startsWith("http"))
            iconurl = imagemap["baseurl"].toString() + icon;

        _numDownloads++;
        ResourceDownload *rd = new ResourceDownload(_netaccess, iconurl, icon);
        connect(rd, SIGNAL(failed()), this, SLOT(downloadIconFailed()));
        connect(rd, SIGNAL(completed()), this, SLOT(downloadIconCompleted()));
        connect(rd, SIGNAL(finished()), this, SLOT(downloadFinished()));
        connect(this, SIGNAL(abortAllDownloads()), rd, SLOT(abortDownload()));
    }
}

void MainWindow::downloadImageJsonFailed()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    if (rd->networkError() != QNetworkReply::NoError) {
        qDebug() << "Getting image JSON failed:" << rd->networkErrorString();
    } else {
        qDebug() << "Getting image JSON failed: HTTP status code" << rd->httpStatusCode();
    }
}

void MainWindow::downloadIconCompleted()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    QPixmap pix;
    pix.loadFromData(rd->data());
    QIcon icon(pix);

    for (QList<QVariantMap>::iterator i = _netImages.begin(); i != _netImages.end(); ++i)
    {
        // Assign icon...
        if (i->value("icon") == rd->saveAs())
            i->insert("iconimage", icon);
    }
}

void MainWindow::downloadIconFailed()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    qDebug() << "Error downloading icon" << rd->urlString();

}

bool MainWindow::orderByIndex(const QVariantMap &m1, const QVariantMap &m2)
{
    QVariant v1 = m1.value("index");
    QVariant v2 = m2.value("index");
    if (v1.isNull() || v2.isNull())
        return false;

    int i1 = v1.value<int>();
    int i2 = v2.value<int>();
    return i1 < i2;
}

void MainWindow::downloadFinished()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    // We can rely on this to get always called, no matter whether there
    // has been an error or not...
    _numDownloads--;

    if (!_numDownloads)
    {
        // Add all images at once, in the right order...
        qSort(_netImages.begin(), _netImages.end(), orderByIndex);
        addImages(_netImages);
        _netImages.clear();

        if (_qpd)
        {
            _qpd->hide();
            _qpd->deleteLater();
            _qpd = NULL;
        }
    }

    rd->deleteLater();
}

QListWidgetItem *MainWindow::findItem(const QVariant &name)
{
    for (int i=0; i<ui->list->count(); i++)
    {
        QListWidgetItem *item = ui->list->item(i);
        QVariantMap m = item->data(Qt::UserRole).toMap();
        if (m.value("name").toString() == name.toString())
        {
            return item;
        }
    }
    return NULL;
}

QList<QListWidgetItem *> MainWindow::selectedItems()
{
    QList<QListWidgetItem *> selected;

    for (int i=0; i < ui->list->count(); i++)
    {
        QListWidgetItem *item = ui->list->item(i);
        if (item->checkState())
        {
            selected.append(item);
        }
    }

    return selected;
}

void MainWindow::updateNeeded()
{
    bool enableOk = false;
    QColor colorNeededLabel = Qt::black;
    bool bold = false;

    _neededMB = 0;

    QListWidgetItem *current = ui->list->currentItem();
    /* If current is valid, we asume its the one we want to install (isSelected() behaves weird) */
    if (current)
    {
        QVariantMap entry = current->data(Qt::UserRole).toMap();
        _neededMB += entry.value("nominal_size").toInt();
        enableOk = true;
    }

    if (!_neededMB)
        ui->labelAmountRequired->setText(tr("Unknown"));
    else
        ui->labelAmountRequired->setText(QString("%2 MB").arg(_neededMB));
    ui->labelAmountAvailable->setText(QString("%2 MB").arg(_availableMB));

    if (_neededMB > _availableMB)
    {
        /* Selection exceeds available space, make label red to alert user */
        colorNeededLabel = Qt::red;
        bold = true;
        enableOk = false;
    }

    ui->actionInstall->setEnabled(enableOk && !_usbGadget->isMassStorage());
    QPalette p = ui->labelAmountRequired->palette();
    if (p.color(QPalette::WindowText) != colorNeededLabel)
    {
        p.setColor(QPalette::WindowText, colorNeededLabel);
        ui->labelAmountRequired->setPalette(p);
    }
    QFont font = ui->labelAmountRequired->font();
    font.setBold(bold);
    ui->labelAmountRequired->setFont(font);
}

void MainWindow::downloadMetaFile(const QString &urlstring, const QString &saveAs)
{
    _numMetaFilesToDownload++;
    ResourceDownload *fd = new ResourceDownload(_netaccess, urlstring, saveAs);
    connect(fd, SIGNAL(completed()), this, SLOT(downloadMetaCompleted()));
    connect(fd, SIGNAL(failed()), this, SLOT(downloadMetaFailed()));
}

void MainWindow::downloadMetaCompleted()
{
    ResourceDownload *fd = qobject_cast<ResourceDownload *>(sender());

    if (fd->saveToFile() < 0) {
        QMessageBox::critical(this, tr("Download error"), tr("Error writing downloaded file to initramfs."), QMessageBox::Close);
        setEnabled(true);
    } else {
        _numMetaFilesToDownload--;
    }
    fd->deleteLater();

    if (_numMetaFilesToDownload == 0) {
        if (_qpd)
        {
            _qpd->hide();
            _qpd->deleteLater();
            _qpd = NULL;
        }
        startImageWrite(_imageEntry);
    }
}

void MainWindow::downloadMetaFailed()
{
    ResourceDownload *fd = qobject_cast<ResourceDownload *>(sender());

    if (_qpd)
    {
        _qpd->hide();
        _qpd->deleteLater();
        _qpd = NULL;
    }
    QMessageBox::critical(this, tr("Download error"), tr("Error downloading meta file")+"\n"+fd->urlString(), QMessageBox::Close);
    reenableImageChoice();

    fd->deleteLater();
}

void MainWindow::reenableImageChoice()
{
    _networkStatusPollTimer.start();
    _mediaPollTimer.start();
    setEnabled(true);
}

void MainWindow::startImageWrite(QVariantMap entry)
{
    /* All meta files downloaded, extract slides tarball, and launch image writer thread */
    _imageWriteThread = new MultiImageWriteThread();
    enum ImageSource imageSource = (enum ImageSource)entry.value("source").toInt();
    QString folder = entry.value("folder").toString();
    QStringList slidesFolders;

    /* Re-mount local media */
    if (!ImageInfo::isNetwork(imageSource))
        mountMedia(entry.value("image_source_blockdev").toString());

    if (entry.contains("license") && !_isAutoinstall) {
        QByteArray text = getFileContents(folder + "/" + entry.value("license").toString());
        ScrollTextDialog eula(entry.value("license_title").toString(), QString(text), QDialogButtonBox::Yes | QDialogButtonBox::Abort);
        eula.setButtonText(QDialogButtonBox::Yes, tr("I Agree"));
        eula.setDefaultButton(QDialogButtonBox::Abort);
        int ret = eula.exec();

        if (ret != QDialogButtonBox::Yes) {
            reenableImageChoice();
            unmountMedia();
            return;
        }
    }

    if (entry.contains("releasenotes") && !_isAutoinstall) {
        QByteArray text = getFileContents(folder + "/" + entry.value("releasenotes").toString());
        ScrollTextDialog eula(entry.value("releasenotes_title").toString(), QString(text), QDialogButtonBox::Ok | QDialogButtonBox::Abort);
        eula.setDefaultButton(QDialogButtonBox::Abort);
        int ret = eula.exec();

        if (ret != QDialogButtonBox::Ok) {
            reenableImageChoice();
            unmountMedia();
            return;
        }
    }

    if (entry.contains("marketing"))
    {
        folder = entry.value("folder").toString();

        QString marketingTar = folder + "/marketing.tar";
        if (QFile::exists(marketingTar))
        {
            /* Extract tarball with slides */
            QProcess::execute("tar xf "+marketingTar+" -C "+folder);
            QFile::remove(marketingTar);
        }
    }

    slidesFolders.clear();
    if (QFile::exists(folder+"/slides_vga"))
    {
        slidesFolders.append(folder+"/slides_vga");
    }

    // Migrate Config Block to default location just before we actually flash...
    if (_toradexConfigBlock->needsWrite) {
        switch (_moduleInformation->storageClass()) {
        case ModuleInformation::StorageClass::Block:
            _toradexConfigBlock->writeToBlockdev(_moduleInformation->configBlockPartition(), _moduleInformation->configBlockOffset());
            break;
        case ModuleInformation::StorageClass::Mtd:
            _toradexConfigBlock->writeToMtddev(_moduleInformation->configBlockPartition(), _moduleInformation->configBlockOffset());
            break;
        }
        qDebug() << "Config Block written to " << _moduleInformation->configBlockPartition();
        _toradexConfigBlock->needsWrite = false;
    }

    _imageWriteThread->setConfigBlock(_toradexConfigBlock);
    _imageWriteThread->setImage(folder, entry.value("image_info").toString(),
                               entry.value("baseurl").toString(), imageSource);

    _psd = new ProgressSlideshowDialog(slidesFolders, "", 20, this);
    connect(_imageWriteThread, SIGNAL(parsedImagesize(qint64)), _psd, SLOT(setMaximum(qint64)));
    connect(_imageWriteThread, SIGNAL(imageProgress(qint64)), _psd, SLOT(updateIOstats(qint64)));
    connect(_imageWriteThread, SIGNAL(completed()), this, SLOT(onCompleted()));
    connect(_imageWriteThread, SIGNAL(error(QString)), this, SLOT(onError(QString)));
    connect(_imageWriteThread, SIGNAL(statusUpdate(QString)), _psd, SLOT(setLabelText(QString)));
    _imageWriteThread->start();
    if (_ld != NULL)
        _ld->hide();
    hide();
    _psd->exec();
}

void MainWindow::on_actionWifi_triggered()
{
    /*
    bool wasAlreadyOnlineBefore = !_networkStatusPollTimer.isActive();

    WifiSettingsDialog wsd;
    if ( wsd.exec() == wsd.Accepted )
    {
        if (wasAlreadyOnlineBefore)
        {
            downloadLists();
        }
    }
    */
}
