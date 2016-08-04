#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "multiimagewritethread.h"
#include "confeditdialog.h"
#include "progressslideshowdialog.h"
#include "config.h"
#include "resourcedownload.h"
#include "languagedialog.h"
#include "scrolltextdialog.h"
#include "json.h"
#include "util.h"
#include "twoiconsdelegate.h"
#include "wifisettingsdialog.h"
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

MainWindow::MainWindow(QSplashScreen *splash, LanguageDialog* ld, QString &toradexProductId, QString &toradexBoardRev,
                       QString &serialNumber, bool allowAutoinstall, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    _qpd(NULL), _toradexProductId(toradexProductId), _toradexBoardRev(toradexBoardRev), _serialNumber(serialNumber),
    _allowAutoinstall(allowAutoinstall), _isAutoinstall(false), _showAll(false), _splash(splash), _ld(ld), _settings(NULL),
    _wasOnline(false), _netaccess(NULL), _mediaMounted(false)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowState(Qt::WindowMaximized);
    setContextMenuPolicy(Qt::NoContextMenu);
    ui->setupUi(this);
    update_window_title();
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
    _internetIcon = QIcon(":/icons/download.png");

    QDir dir;
    dir.mkpath(SRC_MOUNT_FOLDER);

    if (isMounted(SRC_MOUNT_FOLDER))
        unmountMedia();

    /* Disable online help buttons until network is functional */
    ui->actionBrowser->setEnabled(false);

    ui->list->setSelectionMode(QAbstractItemView::SingleSelection);

    _availableMB = (getFileContents("/sys/class/block/mmcblk0/size").trimmed().toULongLong())/2048;

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
    int productId = _toradexProductId.toInt();
    ui->moduleType->setText(tr("Product:") + " " + toradex_modules[productId]);
    ui->moduleVersion->setText(tr("Version:") + " " + _toradexBoardRev);
    ui->moduleSerial->setText(tr("Serial:") + " " + _serialNumber);
}

/* Discover which images we have, and fill in the list */
void MainWindow::showProgressDialog()
{
    /* Ask user to wait while list is populated */
    _qpd = new QProgressDialog(tr("Wait for external media or network..."), QString(), 0, 0, this);
    _qpd->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    _qpd->show();
}

void MainWindow::addImages(QMap<QString,QVariantMap> images)
{
    QSize currentsize = ui->list->iconSize();
    int validImages = 0;

    foreach (QVariant v, images.values())
    {
        QVariantMap m = v.toMap();
        QString name = m.value("name").toString();
        QString description = m.value("description").toString();
        QString version = m.value("version").toString();
        QString releasedate = m.value("release_date").toString();
        QIcon icon = m.value("iconimage").value<QIcon>();
        bool autoinstall = m.value("autoinstall").toBool();
        QVariantList supportedProductIds = m.value("supported_product_ids").toList();
        bool supportedImage = supportedProductIds.contains(_toradexProductId);

        /* We found an auto install image, immediately start flashing it... */
        if (_allowAutoinstall && autoinstall && supportedImage) {
            _isAutoinstall = true;
            installImage(m);
            return;
        }

        QString friendlyname = name;
        if (!version.isEmpty()) {
            friendlyname += " (" + version;
            if (!releasedate.isEmpty())
                friendlyname += ", " + releasedate;
            friendlyname += ")";
        }
        if (!description.isEmpty())
            friendlyname += "\n"+description;

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
        QListWidgetItem *item = new QListWidgetItem(icon, friendlyname);
        item->setData(Qt::UserRole, m);

        if (supportedImage)
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        else
            item->setFlags(Qt::NoItemFlags);

        QVariant source = m.value("source");
        if (source == SOURCE_USB)
            item->setData(SecondIconRole, _usbIcon);
        else if (source == SOURCE_SDCARD)
            item->setData(SecondIconRole, _sdIcon);
        else if (source == SOURCE_NETWORK)
            item->setData(SecondIconRole, _internetIcon);

        ui->list->addItem(item);
        validImages++;
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

    ui->list->setCurrentRow(0);
    updateNeeded();
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
        if (entry.value("source") == source) {
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
        if (removable.startsWith("1")) {
            /* Get partitions of this device */
            QDir devdir("/dev", blockdev + "?*", QDir::Name, QDir::System | QDir::NoDotAndDotDot);
            QStringList devfiles = devdir.entryList();
            if (devfiles.isEmpty()) {
                /* Raw blockdev without any partition table? Try to use that... */
                processMedia(SOURCE_USB, blockdev);
            } else {
                /* Everything else... */
                foreach (QString devfile, devfiles) {
                    processMedia(SOURCE_USB, devfile);
                }
            }
        }
    }
}

void MainWindow::checkSDcard()
{
    QDir dir("/sys/block/", "mmcblk*", QDir::Name, QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList list = dir.entryList();

    // Unfortunately we can't rely on removable property for SD card... just ignore mmcblk0 and take everything else
    foreach(QString blockdev, list) {
        if (blockdev.startsWith("mmcblk0"))
            continue;

        /* Get first partition of this device */
        QDir devdir("/dev", blockdev + "p?", QDir::Name, QDir::System | QDir::NoDotAndDotDot);
        QStringList devfiles = devdir.entryList();
        if (devfiles.isEmpty()) {
            /* Raw blockdev without any partition table? Try to use that... */
            processMedia(SOURCE_SDCARD, blockdev);
        } else {
            /* Everything else... */
            foreach (QString devfile, devfiles) {
                processMedia(SOURCE_SDCARD, devfile);
            }
        }
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

    qDebug() << "Reading images from media" << blockdevpath;
    if (_qpd) {
        _qpd->setLabelText(tr("Reading images from device %1...").arg(blockdevpath));
        QApplication::processEvents();
    }

    if (mountMedia(blockdevpath))
    {
        QMap<QString,QVariantMap> images = listMediaImages(SRC_MOUNT_FOLDER, blockdevpath, src);
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

    /* After first poll, set intervall to 1s */
    _mediaPollTimer.setInterval(1000);
}

QMap<QString, QVariantMap> MainWindow::listMediaImages(const QString &path, const QString &blockdev, enum ImageSource source)
{
    QMap<QString,QVariantMap> images;

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

        QString basename = imagemap.value("name").toString();
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
        images[basename] = imagemap;
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
    setEnabled(false);
    _numMetaFilesToDownload = 0;

    /* Stop any polling, we are about to install a image */
    _networkStatusPollTimer.stop();
    _mediaPollTimer.stop();

    if (entry.value("source") == SOURCE_NETWORK)
    {
        _imageEntry = entry;

        QString folder = entry.value("folder").toString();
        QString url = entry.value("baseurl").toString();

        if (entry.contains("marketing_info"))
            downloadMetaFile(url + entry.value("marketing_info").toString(), folder+"/marketing.tar");

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

void MainWindow::on_actionCancel_triggered()
{
    close();
}

void MainWindow::onCompleted()
{
    _psd->hide();

    if (!_isAutoinstall) {
        QMessageBox msgbox(QMessageBox::Information,
                           tr("Image Installed"),
                           tr("Image installed successfully.") + "\n\n" + tr("In case recovery mode has been used a power cycle will be necessary."), QMessageBox::Ok, this);
        msgbox.button(QMessageBox::Ok)->setText(tr("Restart"));
        msgbox.exec();
    }
    _psd->deleteLater();
    _psd = NULL;
    close();
}

void MainWindow::onError(const QString &msg)
{
    qDebug() << "Error:" << msg;
    if (_qpd)
        _qpd->hide();
    QMessageBox::critical(this, tr("Error"), msg, QMessageBox::Close);
    setEnabled(true);
}

void MainWindow::onQuery(const QString &msg, const QString &title, QMessageBox::StandardButton* answer)
{
    *answer = QMessageBox::question(this, title, msg, QMessageBox::Yes|QMessageBox::No);
}

void MainWindow::update_window_title()
{
    setWindowTitle(QString(tr("Tez-i v%1 - Built: %2")).arg(VERSION_NUMBER, QString::fromLocal8Bit(__DATE__)));
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event && event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
        update_window_title();
        updateModuleInformation();
        updateNeeded();
        //repopulate();
    }

    QMainWindow::changeEvent(event);
}

void MainWindow::inputSequence()
{
    QLabel* info = new QLabel(this);
    info->setPixmap(QPixmap("/usr/data"));
    info->setGeometry(0,0,640,480);
    info->show();
}

void MainWindow::on_actionAdvanced_triggered(bool checked)
{
    ui->advToolBar->setVisible(checked);
}

void MainWindow::on_actionEdit_config_triggered()
{
    /* If no installed OS is selected, default to first extended partition */
    QString partition = FAT_PARTITION_OF_IMAGE;
    QListWidgetItem *item = ui->list->currentItem();

    if (item && item->data(Qt::UserRole).toMap().contains("partitions"))
    {
        QVariantList l = item->data(Qt::UserRole).toMap().value("partitions").toList();
        if (!l.isEmpty())
            partition = l.first().toString();
    }

    ConfEditDialog d(partition);
    d.exec();
}

void MainWindow::on_actionBrowser_triggered()
{
    startBrowser();
}

bool MainWindow::requireNetwork()
{
    if (!isOnline())
    {
        QMessageBox::critical(this,
                              tr("No network access"),
                              tr("Wired network access is required for this feature. Please insert a network cable into the network port."),
                              QMessageBox::Close);
        return false;
    }

    return true;
}

void MainWindow::startBrowser()
{
    if (!requireNetwork())
        return;
    QProcess *proc = new QProcess(this);
    QString lang = LanguageDialog::instance("en", "gb")->currentLanguage();
    if (lang == "gb" || lang == "us" || lang == "")
        lang = "en";
    proc->start("arora -lang "+lang+" "+HOMEPAGE);
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

bool MainWindow::isOnline()
{
    /* Check if we have an IP-address other than localhost */
    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();

    foreach (QHostAddress a, addresses)
    {
        if (a != QHostAddress::LocalHost && a != QHostAddress::LocalHostIPv6 &&
            a.scopeId() == "") {
            return true;
        }
    }

    return false;
}

void MainWindow::pollNetworkStatus()
{
    if (isOnline()) {
        if (!_wasOnline) {
            onOnlineStateChanged(true);
            _wasOnline = true;
        }
    } else {
        if (_wasOnline) {
            onOnlineStateChanged(false);
            _wasOnline = false;
        }
    }
}

void MainWindow::onOnlineStateChanged(bool online)
{
    if (online) {
        qDebug() << "Network up in" << _time.elapsed()/1000.0 << "seconds";
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

        /* Download list of images from static URLs... */
        downloadLists();

        //ui->actionBrowser->setEnabled(true);
        emit networkUp();
    } else {
        removeImagesBySource(SOURCE_NETWORK);
    }
}

void MainWindow::downloadLists()
{
    _numIconsToDownload = 0;
    QStringList urls = QString(DEFAULT_IMAGE_SERVER).split(' ', QString::SkipEmptyParts);
    if (_qpd)
        _qpd->setLabelText(tr("Downloading image list from Internet..."));

    foreach (QString url, urls)
    {
        ResourceDownload *rd = new ResourceDownload(_netaccess, url, NULL);
        connect(rd, SIGNAL(failed()), this, SLOT(downloadListJsonFailed()));
        connect(rd, SIGNAL(completed()), this, SLOT(downloadListJsonCompleted()));
    }
}

void MainWindow::downloadListJsonCompleted()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    QVariant json = Json::parse(rd->data());
    rd->deleteLater();
    if (json.isNull()) {
        QMessageBox::critical(this, tr("Error"), tr("Error parsing list JSON downloaded from server"), QMessageBox::Close);
        return;
    }

    QVariantList list = json.toMap().value("images").toList();
    QString sourceurlpath = getUrlPath(rd->urlString());

    foreach (QVariant image, list)
    {
        QString url = image.toString();

        // Relative URL...
        if (!url.startsWith("http"))
            url = sourceurlpath + url;

        ResourceDownload *rd = new ResourceDownload(_netaccess, url, NULL);
        connect(rd, SIGNAL(failed()), this, SLOT(downloadImageJsonFailed()));
        connect(rd, SIGNAL(completed()), this, SLOT(downloadImageJsonCompleted()));
    }
}


void MainWindow::downloadListJsonFailed()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    if (_qpd)
        _qpd->hide();

    if (rd->networkError() != QNetworkReply::NoError) {
        QMessageBox::critical(this, tr("Download error"), tr("Error downloading image list from Internet: %1").arg(rd->networkErrorString()), QMessageBox::Close);
    } else {
        QMessageBox::critical(this, tr("Download error"), tr("Error downloading image list from Internet: HTTP status code %1").arg(rd->httpStatusCode()), QMessageBox::Close);
    }

    rd->deleteLater();
}

void MainWindow::downloadImageJsonCompleted()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    QByteArray json = rd->data();
    QVariantMap imagemap = Json::parse(json).toMap();

    QString basename = imagemap.value("name").toString();
    QDir d;
    QString folder = "/var/volatile/" + basename.replace(' ', '_');
    if (!d.exists(folder))
        d.mkpath(folder);
    imagemap["folder"] = folder;
    imagemap["source"] = SOURCE_NETWORK;

    if (!imagemap.contains("nominal_size"))
        imagemap["nominal_size"] = calculateNominalSize(imagemap);

    QFile imageinfo(folder + "/image.json");
    imageinfo.open(QIODevice::WriteOnly | QIODevice::Text);
    imageinfo.write(json);
    imageinfo.close();
    imagemap["image_info"] = "image.json";
    imagemap["baseurl"] = getUrlPath(rd->urlString());
    QMap<QString,QVariantMap> images;
    images[basename] = imagemap;

    addImages(images);

    QString icon = imagemap.value("icon").toString();
    if (!icon.isNull()) {
        QString iconurl = icon;
        if (!icon.startsWith("http"))
            iconurl = imagemap["baseurl"].toString() + icon;

        ResourceDownload *rd = new ResourceDownload(_netaccess, iconurl, icon);
        connect(rd, SIGNAL(failed()), this, SLOT(downloadIconFailed()));
        connect(rd, SIGNAL(completed()), this, SLOT(downloadIconCompleted()));
    }

    rd->deleteLater();
}

void MainWindow::downloadImageJsonFailed()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    if (rd->networkError() != QNetworkReply::NoError) {
        qDebug() << "Getting image JSON failed:" << rd->networkErrorString();
    } else {
        qDebug() << "Getting image JSON failed: HTTP status code" << rd->httpStatusCode();
    }

    rd->deleteLater();
}

void MainWindow::downloadIconCompleted()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    QPixmap pix;
    pix.loadFromData(rd->data());
    QIcon icon(pix);

    for (int i=0; i<ui->list->count(); i++)
    {
        QVariantMap m = ui->list->item(i)->data(Qt::UserRole).toMap();
        ui->list->setIconSize(QSize(40, 40));
        if (m.value("icon") == rd->saveAs() && m.value("source") == SOURCE_NETWORK)
        {
            ui->list->item(i)->setIcon(icon);
        }
    }

    if (--_numIconsToDownload == 0 && _qpd)
    {
        _qpd->hide();
        _qpd->deleteLater();
        _qpd = NULL;
    }

    rd->deleteLater();
}

void MainWindow::downloadIconFailed()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    qDebug() << "Error downloading icon" << rd->urlString();
    if (--_numIconsToDownload == 0 && _qpd)
    {
        _qpd->hide();
        _qpd->deleteLater();
        _qpd = NULL;
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

    ui->neededLabel->setText(QString("%1: %2 MB").arg(tr("Needed"), QString::number(_neededMB)));
    ui->availableLabel->setText(QString("%1: %2 MB").arg(tr("Available"), QString::number(_availableMB)));

    if (_neededMB > _availableMB)
    {
        /* Selection exceeds available space, make label red to alert user */
        colorNeededLabel = Qt::red;
        bold = true;
        enableOk = false;
    }

    ui->actionInstall->setEnabled(enableOk);
    QPalette p = ui->neededLabel->palette();
    if (p.color(QPalette::WindowText) != colorNeededLabel)
    {
        p.setColor(QPalette::WindowText, colorNeededLabel);
        ui->neededLabel->setPalette(p);
    }
    QFont font = ui->neededLabel->font();
    font.setBold(bold);
    ui->neededLabel->setFont(font);
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
        QMessageBox::critical(this, tr("Download error"), tr("Error writing downloaded file to SD card. SD card or file system may be damaged."), QMessageBox::Close);
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
    setEnabled(true);
    fd->deleteLater();
}

void MainWindow::startImageWrite(QVariantMap entry)
{
    /* All meta files downloaded, extract slides tarball, and launch image writer thread */
    MultiImageWriteThread *imageWriteThread = new MultiImageWriteThread();
    QString folder = entry.value("folder").toString();
    QStringList slidesFolders;

    if (entry.contains("license")) {
        QByteArray text = getFileContents(folder + "/" + entry.value("license").toString());
        ScrollTextDialog eula(entry.value("license_title").toString(), QString(text), QDialogButtonBox::Yes | QDialogButtonBox::Abort);
        eula.setDefaultButton(QDialogButtonBox::No);
        int ret = eula.exec();

        if (ret != QDialogButtonBox::Yes) {
            _networkStatusPollTimer.start();
            _mediaPollTimer.start();
            setEnabled(true);
            return;
        }
    }

    /* Re-mount local media */
    if (entry.value("source") != SOURCE_NETWORK)
        mountMedia(entry.value("image_source_blockdev").toString());

    if (entry.contains("marketing_info"))
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

    //QRect s = QApplication::desktop()->screenGeometry();
    //if (s.width() > 640 && QFile::exists(folder+"/slides"))
    //{
    //    slidesFolder = folder+"/slides";
    //}
    slidesFolders.clear();
    if (QFile::exists(folder+"/slides_vga"))
    {
        slidesFolders.append(folder+"/slides_vga");
    }

    imageWriteThread->setImage(folder, entry.value("image_info").toString(),
                               entry.value("baseurl").toString(), (enum ImageSource)entry.value("source").toInt());

    _psd = new ProgressSlideshowDialog(slidesFolders, "", 15, this);
    connect(imageWriteThread, SIGNAL(parsedImagesize(qint64)), _psd, SLOT(setMaximum(qint64)));
    connect(imageWriteThread, SIGNAL(imageProgress(qint64)), _psd, SLOT(updateIOstats(qint64)));
    connect(imageWriteThread, SIGNAL(completed()), this, SLOT(onCompleted()));
    connect(imageWriteThread, SIGNAL(error(QString)), this, SLOT(onError(QString)));
    connect(imageWriteThread, SIGNAL(statusUpdate(QString)), _psd, SLOT(setLabelText(QString)));
    imageWriteThread->start();
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
