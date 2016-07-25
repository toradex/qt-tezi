#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "multiimagewritethread.h"
#include "confeditdialog.h"
#include "progressslideshowdialog.h"
#include "config.h"
#include "languagedialog.h"
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

/* Flag to keep track wheter or not we already repartitioned. */
bool MainWindow::_partInited = false;

/* Flag to keep track of current display mode. */
int MainWindow::_currentMode = 0;

MainWindow::MainWindow(const QString &defaultDisplay, QSplashScreen *splash, QString &toradexProductId, QString &toradexBoardRev,
                       bool allowAutoinstall, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    _qpd(NULL), _defaultDisplay(defaultDisplay), _toradexProductId(toradexProductId), _toradexBoardRev(toradexBoardRev),
    _allowAutoinstall(allowAutoinstall), _isAutoinstall(false), _showAll(false), _splash(splash), _settings(NULL),
    _hasWifi(false), _netaccess(NULL), _mediaMounted(false)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    setContextMenuPolicy(Qt::NoContextMenu);
    update_window_title();
    ui->list->setItemDelegate(new TwoIconsDelegate(this));
    ui->advToolBar->setVisible(false);

    QRect s = QApplication::desktop()->screenGeometry();
    if (s.height() < 500)
        resize(s.width()-10, s.height()-100);

    _model = getFileContents("/proc/device-tree/model");

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
    dir.mkdir(SRC_MOUNT_FOLDER);

    if (isMounted(SRC_MOUNT_FOLDER))
        unmountMedia();

    /* Disable online help buttons until network is functional */
    ui->actionBrowser->setEnabled(false);

    ui->list->setSelectionMode(QAbstractItemView::SingleSelection);

    _availableMB = (getFileContents("/sys/class/block/mmcblk0/size").trimmed().toULongLong())/2048;

    connect(&_mediaPollTimer, SIGNAL(timeout()), SLOT(pollMedia()));
    _mediaPollTimer.start(100);
    //QTimer::singleShot(1, this, SLOT(populate()));

    startNetworking();

}

MainWindow::~MainWindow()
{
    QProcess::execute("umount " SRC_MOUNT_FOLDER);
    delete ui;
}

/* Discover which images we have, and fill in the list */
void MainWindow::populate()
{
    /* Ask user to wait while list is populated */
    _qpd = new QProgressDialog(tr("Wait for external media or network..."), QString(), 0, 0, this);
    _qpd->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    _qpd->show();
/*
    int timeout = 5000;
    if (getFileContents("/settings/wpa_supplicant.conf").contains("ssid="))
    {
        // Longer timeout if we have a wifi network configured
        timeout = 8000;
    }
    QTimer::singleShot(timeout, this, SLOT(hideDialogIfNoNetwork()));
    */

    // Fill in list of images
    /*
    repopulate();

    if (ui->list->count() != 0)
    {
        QList<QListWidgetItem *> l = ui->list->findItems(RECOMMENDED_IMAGE, Qt::MatchExactly);

        if (!l.isEmpty())
        {
            ui->list->setCurrentItem(l.first());
        }
        else
        {
            ui->list->setCurrentRow(0);
        }

        if (_allowSilent && !QFile::exists(FAT_PARTITION_OF_IMAGE) && ui->list->count() == 1)
        {
            // No OS installed, perform silent installation
            qDebug() << "Performing silent installation";
            _silent = true;
            ui->list->item(0)->setCheckState(Qt::Checked);
            on_actionInstall_triggered();
        }
    }

    _qpd->hide();
    _qpd->deleteLater();
    _qpd = NULL;

    bool osInstalled = QFile::exists(FAT_PARTITION_OF_IMAGE);
    ui->actionCancel->setEnabled(osInstalled);*/
}

void MainWindow::addImages(QMap<QString,QVariantMap> images)
{
    QSize currentsize = ui->list->iconSize();

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

        if (!icon.isNull())
        {
            QList<QSize> avs = icon.availableSizes();
            if (avs.isEmpty())
            {
                /* Icon file corrupt */
                icon = QIcon();
            }
            else
            {
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

    ui->actionCancel->setEnabled(true);

    /* Hide progress dialog since we have images we could use now... */
    if (_qpd && images.count()) {
        _qpd->hide();
        _qpd->deleteLater();
        _qpd = NULL;
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

        if (!images.isEmpty())
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

    /* Local image folders */
    QDir dir(path, "", QDir::Name, QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList list = dir.entryList();

    foreach(QString image, list) {
        QString imagefolder = path + QDir::separator() + image;
        QString imagejson = imagefolder + QDir::separator() + "image.json";
        if (!QFile::exists(imagejson))
            continue;
        QVariantMap imagemap = Json::loadFromFile(imagejson).toMap();

        QString basename = imagemap.value("name").toString();
        imagemap["folder"] = imagefolder;
        imagemap["source"] = source;

        if (!imagemap.contains("nominal_size")) {
            // Calculate nominal_size based on partition information
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

            imagemap["nominal_size"] = nominal_size;
        }

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

void MainWindow::on_list_currentItemChanged(QListWidgetItem * current, QListWidgetItem * previous)
{
    updateNeeded();
}

void MainWindow::installImage(QVariantMap entry)
{
    _numMetaFilesToDownload = 0;

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
        setEnabled(false);
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

    if (!_isAutoinstall)
        QMessageBox::information(this,
                                 tr("Image Installed"),
                                 tr("Image installed successfully"), QMessageBox::Ok);
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
    /*
    QFile f("/settings/wpa_supplicant.conf");

    if ( f.exists() && f.size() == 0 )
    {
        // Remove corrupt file
        f.remove();
    }
    if ( !f.exists() )
    {
        // If user supplied a wpa_supplicant.conf on the FAT partition copy that one to settings
        //   otherwise copy the default one stored in the initramfs
        if (QFile::exists("/mnt/wpa_supplicant.conf"))
            QFile::copy("/mnt/wpa_supplicant.conf", "/settings/wpa_supplicant.conf");
        else
        {
            qDebug() << "Copying /etc/wpa_supplicant.conf to /settings/wpa_supplicant.conf";
            QFile::copy("/etc/wpa_supplicant.conf", "/settings/wpa_supplicant.conf");
        }
    }
    QFile::remove("/etc/wpa_supplicant.conf");

    // Enable dbus so that we can use it to talk to wpa_supplicant later
    qDebug() << "Starting dbus";
    QProcess::execute("/etc/init.d/S30dbus start");
*/

    /* Run dhcpcd in background */
    /*
    QProcess *proc = new QProcess(this);
    qDebug() << "Starting dhcpcd";
    proc->start("/sbin/dhcpcd --noarp -e wpa_supplicant_conf=/settings/wpa_supplicant.conf --denyinterfaces \"*_ap\"");
    */
    QProcess *proc = new QProcess(this);
    qDebug() << "Starting dhcpcd";
    proc->start("/sbin/udhcpc");

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
        if (a != QHostAddress::LocalHost && a != QHostAddress::LocalHostIPv6)
            return true;
    }

    return false;
}

void MainWindow::pollNetworkStatus()
{
    if (!_hasWifi && QFile::exists("/sys/class/net/wlan0"))
    {
        _hasWifi = true;
        ui->actionWifi->setEnabled(true);
    }
    if (isOnline())
    {
        _networkStatusPollTimer.stop();
        onOnlineStateChanged(true);
    }
}

void MainWindow::onOnlineStateChanged(bool online)
{
    if (online)
    {
        qDebug() << "Network up in" << _time.elapsed()/1000.0 << "seconds";
        if (!_netaccess)
        {
            QDir dir;
            dir.mkdir("/settings/cache");
            _netaccess = new QNetworkAccessManager(this);
            QNetworkDiskCache *_cache = new QNetworkDiskCache(this);
            _cache->setCacheDirectory("/settings/cache");
            _cache->setMaximumCacheSize(8 * 1024 * 1024);
            _netaccess->setCache(_cache);
            QNetworkConfigurationManager manager;
            _netaccess->setConfiguration(manager.defaultConfiguration());

            downloadLists();
        }
        //ui->actionBrowser->setEnabled(true);
        emit networkUp();
    }
}

void MainWindow::downloadList(const QString &urlstring)
{
    QNetworkReply *reply = _netaccess->get(QNetworkRequest(QUrl(urlstring)));
    reply->setProperty("isList", true);
    connect(reply, SIGNAL(finished()), this, SLOT(downloadListRedirectCheck()));
}

void MainWindow::downloadLists()
{
    _numIconsToDownload = 0;
    QStringList urls = QString(DEFAULT_REPO_SERVER).split(' ', QString::SkipEmptyParts);

    foreach (QString url, urls)
    {
        downloadList(url);
    }
}

void MainWindow::downloadListComplete()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    int httpstatuscode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    /* Set our clock to server time if we currently have a date before 2015 */
    QByteArray dateStr = reply->rawHeader("Date");
    if (!dateStr.isEmpty() && QDate::currentDate().year() < 2015)
    {
        // Qt 4 does not have a standard function for parsing the Date header, but it does
        // have one for parsing a Last-Modified header that uses the same date/time format, so just use that
        QNetworkRequest dummyReq;
        dummyReq.setRawHeader("Last-Modified", dateStr);
        QDateTime parsedDate = dummyReq.header(dummyReq.LastModifiedHeader).toDateTime();

        struct timeval tv;
        tv.tv_sec = parsedDate.toTime_t();
        tv.tv_usec = 0;
        settimeofday(&tv, NULL);
    }

    if (reply->error() != reply->NoError || httpstatuscode < 200 || httpstatuscode > 399)
    {
        if (_qpd)
            _qpd->hide();
        QMessageBox::critical(this, tr("Download error"), tr("Error downloading distribution list from Internet"), QMessageBox::Close);
    }
    else
    {
        QVariant json = Json::parse(reply->readAll());
        if (json.isNull()) {
            QMessageBox::critical(this, tr("Error"), tr("Error parsing list.json downloaded from server"), QMessageBox::Close);
        } else {
            processImageList(reply->url(), json);
        }
    }

    reply->deleteLater();
}

void MainWindow::downloadImageComplete()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    int httpstatuscode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != reply->NoError) {
        qDebug() << "Getting image JSON failed:" << reply->url();
        return;
    }
    QByteArray json = reply->readAll();
    QVariantMap imagemap = Json::parse(json).toMap();

    QString basename = imagemap.value("name").toString();
    QDir d;
    QString folder = "/var/volatile/" + basename.replace(' ', '_');
    if (!d.exists(folder))
        d.mkpath(folder);
    imagemap["folder"] = folder;
    imagemap["source"] = SOURCE_NETWORK;

    if (!imagemap.contains("nominal_size")) {
        // Calculate nominal_size based on partition information
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

        imagemap["nominal_size"] = nominal_size;
    }

    QFile imageinfo(folder + "/image.json");
    imageinfo.open(QIODevice::WriteOnly | QIODevice::Text);
    imageinfo.write(json);
    imageinfo.close();
    imagemap["image_info"] = "image.json";
    imagemap["baseurl"] = getUrlPath(reply->url().toString());
    QMap<QString,QVariantMap> images;
    images[basename] = imagemap;

    addImages(images);

    QString icon = imagemap.value("icon").toString();
    if (!icon.isNull()) {
        QString iconurl = icon;
        if (!icon.startsWith("http"))
            iconurl = imagemap["baseurl"].toString() + icon;

        downloadIcon(iconurl, icon);
    }
}



void MainWindow::processImageList(QUrl sourceurl, QVariant json)
{
    //QSet<QString> iconurls;
    QVariantList list = json.toMap().value("images").toList();
    QString sourceurlpath = getUrlPath(sourceurl.toString());

    foreach (QVariant image, list)
    {
        QString url = image.toString();

        // Relative URL...
        if (!url.startsWith("http"))
            url = sourceurlpath + url;

        QNetworkReply *reply = _netaccess->get(QNetworkRequest(QUrl(url)));
        connect(reply, SIGNAL(finished()), this, SLOT(downloadListRedirectCheck()));
    }
}

void MainWindow::downloadIcon(const QString &urlstring, const QString &originalurl)
{
    QUrl url(urlstring);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::User, originalurl);
    QNetworkReply *reply = _netaccess->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(downloadIconRedirectCheck()));
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

void MainWindow::downloadIconComplete()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString url = reply->url().toString();
    QString originalurl = reply->request().attribute(QNetworkRequest::User).toString();
    int httpstatuscode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != reply->NoError || httpstatuscode < 200 || httpstatuscode > 399)
    {
        //QMessageBox::critical(this, tr("Download error"), tr("Error downloading icon '%1'").arg(reply->url().toString()), QMessageBox::Close);
        qDebug() << "Error downloading icon" << url;
    }
    else
    {
        QPixmap pix;
        pix.loadFromData(reply->readAll());
        QIcon icon(pix);

        for (int i=0; i<ui->list->count(); i++)
        {
            QVariantMap m = ui->list->item(i)->data(Qt::UserRole).toMap();
            ui->list->setIconSize(QSize(40,40));
            if (m.value("icon") == originalurl && m.value("source") == SOURCE_NETWORK)
            {
                ui->list->item(i)->setIcon(icon);
            }
        }
    }
    if (--_numIconsToDownload == 0 && _qpd)
    {
        _qpd->hide();
        _qpd->deleteLater();
        _qpd = NULL;
    }

    reply->deleteLater();
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
    qDebug() << "Downloading" << urlstring << "to" << saveAs;
    _numMetaFilesToDownload++;
    QUrl url(urlstring);
    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::User, saveAs);
    QNetworkReply *reply = _netaccess->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(downloadMetaRedirectCheck()));
}

void MainWindow::downloadListRedirectCheck()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    int httpstatuscode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString redirectionurl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();

    if (httpstatuscode > 300 && httpstatuscode < 400)
    {
        qDebug() << "Redirection - Re-trying download from" << redirectionurl;
        downloadList(redirectionurl);
    }
    else if (reply->property("isList").toBool())
        downloadListComplete();
    else
        downloadImageComplete();
}

void MainWindow::downloadIconRedirectCheck()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    int httpstatuscode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString redirectionurl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();
    QString originalurl = reply->request().attribute(QNetworkRequest::User).toString();;

    if (httpstatuscode > 300 && httpstatuscode < 400)
    {
        qDebug() << "Redirection - Re-trying download from" << redirectionurl;
        downloadIcon(redirectionurl, originalurl);
    }
    else
        downloadIconComplete();
}

void MainWindow::downloadMetaRedirectCheck()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    int httpstatuscode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString redirectionurl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();
    QString saveAs = reply->request().attribute(QNetworkRequest::User).toString();

    if (httpstatuscode > 300 && httpstatuscode < 400)
    {
        qDebug() << "Redirection - Re-trying download from" << redirectionurl;
        _numMetaFilesToDownload--;
        downloadMetaFile(redirectionurl, saveAs);
    }
    else
        downloadMetaComplete();
}

void MainWindow::downloadMetaComplete()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    int httpstatuscode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != reply->NoError || httpstatuscode < 200 || httpstatuscode > 399)
    {
        if (_qpd)
        {
            _qpd->hide();
            _qpd->deleteLater();
            _qpd = NULL;
        }
        QMessageBox::critical(this, tr("Download error"), tr("Error downloading meta file")+"\n"+reply->url().toString(), QMessageBox::Close);
        setEnabled(true);
    }
    else
    {
        QString saveAs = reply->request().attribute(QNetworkRequest::User).toString();
        QFile f(saveAs);
        f.open(f.WriteOnly);
        if (f.write(reply->readAll()) == -1)
        {
            QMessageBox::critical(this, tr("Download error"), tr("Error writing downloaded file to SD card. SD card or file system may be damaged."), QMessageBox::Close);
            setEnabled(true);
        }
        else
        {
            _numMetaFilesToDownload--;
        }
        f.close();
    }

    if (_numMetaFilesToDownload == 0)
    {
        if (_qpd)
        {
            _qpd->hide();
            _qpd->deleteLater();
            _qpd = NULL;
        }
        startImageWrite(_imageEntry);
    }
}

void MainWindow::startImageWrite(QVariantMap entry)
{
    /* All meta files downloaded, extract slides tarball, and launch image writer thread */
    MultiImageWriteThread *imageWriteThread = new MultiImageWriteThread();
    QString folder, slidesFolder;
    QStringList slidesFolders;

    /* Stop media poller... */
    _mediaPollTimer.stop();

    /* Re-mount local media */
    mountMedia(entry.value("image_source_blockdev").toString());

    if (entry.contains("folder"))
    {
        /* Local image */
        folder = entry.value("folder").toString();
    }
    else
    {
        folder = "/var/volatile/"+entry.value("name").toString();
        folder.replace(' ', '_');

        QString marketingTar = folder+"/marketing.tar";
        if (QFile::exists(marketingTar))
        {
            /* Extract tarball with slides */
            QProcess::execute("tar xf "+marketingTar+" -C "+folder);
            QFile::remove(marketingTar);
        }
    }

    slidesFolder.clear();
    //QRect s = QApplication::desktop()->screenGeometry();
    //if (s.width() > 640 && QFile::exists(folder+"/slides"))
    //{
    //    slidesFolder = folder+"/slides";
    //}
    if (QFile::exists(folder+"/slides_vga"))
    {
        slidesFolder = folder+"/slides_vga";
    }
    imageWriteThread->setImage(folder, entry.value("image_info").toString(),
                               entry.value("baseurl").toString(), (enum ImageSource)entry.value("source").toInt());
    if (!slidesFolder.isEmpty())
        slidesFolders.append(slidesFolder);

    if (slidesFolders.isEmpty())
        slidesFolder.append("/mnt/defaults/slides");

    _psd = new ProgressSlideshowDialog(slidesFolders, "", 20, this);
    connect(imageWriteThread, SIGNAL(parsedImagesize(qint64)), _psd, SLOT(setMaximum(qint64)));
    connect(imageWriteThread, SIGNAL(imageProgress(qint64)), _psd, SLOT(updateIOstats(qint64)));
    connect(imageWriteThread, SIGNAL(completed()), this, SLOT(onCompleted()));
    connect(imageWriteThread, SIGNAL(error(QString)), this, SLOT(onError(QString)));
    connect(imageWriteThread, SIGNAL(statusUpdate(QString)), _psd, SLOT(setLabelText(QString)));
    imageWriteThread->start();
    hide();
    _psd->exec();
}

void MainWindow::hideDialogIfNoNetwork()
{
    if (_qpd)
    {
        if (!isOnline())
        {
            /* No network cable inserted */
            _qpd->hide();
            _qpd->deleteLater();
            _qpd = NULL;

            if (ui->list->count() == 0)
            {
                /* No local images either */
                if (_hasWifi)
                {
                    QMessageBox::critical(this,
                                          tr("No network access"),
                                          tr("Network access is required to use NOOBS without local images. Please select your wifi network in the next screen."),
                                          QMessageBox::Close);
                    on_actionWifi_triggered();
                }
                else
                {
                    QMessageBox::critical(this,
                                          tr("No network access"),
                                          tr("Wired network access is required to use NOOBS without local images. Please insert a network cable into the network port."),
                                          QMessageBox::Close);
                }
            }
        }
    }
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
