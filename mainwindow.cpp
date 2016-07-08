#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "multiimagewritethread.h"
#include "initdrivethread.h"
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

#define MEDIA_USB "/dev/sda1"
#define MEDIA_SDCARD "/dev/mmcblk1p1"

/* Flag to keep track wheter or not we already repartitioned. */
bool MainWindow::_partInited = false;

/* Flag to keep track of current display mode. */
int MainWindow::_currentMode = 0;

MainWindow::MainWindow(const QString &defaultDisplay, QSplashScreen *splash, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    _qpd(NULL), _kcpos(0), _defaultDisplay(defaultDisplay),
    _silent(false), _allowSilent(false), _showAll(false), _splash(splash), _settings(NULL),
    _hasWifi(false), _netaccess(NULL)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    setContextMenuPolicy(Qt::NoContextMenu);
    update_window_title();
    _kc << 0x01000013 << 0x01000013 << 0x01000015 << 0x01000015 << 0x01000012
        << 0x01000014 << 0x01000012 << 0x01000014 << 0x42 << 0x41;
    ui->list->setItemDelegate(new TwoIconsDelegate(this));
    ui->advToolBar->setVisible(false);

    QRect s = QApplication::desktop()->screenGeometry();
    if (s.height() < 500)
        resize(s.width()-10, s.height()-100);
/*
    if (qApp->arguments().contains("-runinstaller") && !_partInited)
    {
        // Repartition SD card first
        _partInited = true;
        setEnabled(false);
        _qpd = new QProgressDialog( tr("Setting up SD card"), QString(), 0, 0, this);
        _qpd->setWindowModality(Qt::WindowModal);
        _qpd->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);

        InitDriveThread *idt = new InitDriveThread(this);
        connect(idt, SIGNAL(statusUpdate(QString)), _qpd, SLOT(setLabelText(QString)));
        connect(idt, SIGNAL(completed()), _qpd, SLOT(deleteLater()));
        connect(idt, SIGNAL(error(QString)), this, SLOT(onError(QString)));
        connect(idt, SIGNAL(query(QString, QString, QMessageBox::StandardButton*)),
                this, SLOT(onQuery(QString, QString, QMessageBox::StandardButton*)),
                Qt::BlockingQueuedConnection);

        idt->start();
        _qpd->exec();
        setEnabled(true);
    }

    // Make sure the SD card is ready, and partition table is read by Linux
    if (!QFile::exists(SETTINGS_PARTITION))
    {
        _qpd = new QProgressDialog( tr("Waiting for SD card (settings partition)"), QString(), 0, 0, this);
        _qpd->setWindowModality(Qt::WindowModal);
        _qpd->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
        _qpd->show();

        while (!QFile::exists(SETTINGS_PARTITION))
        {
            QApplication::processEvents(QEventLoop::WaitForMoreEvents, 250);
        }
        _qpd->hide();
        _qpd->deleteLater();
    }

    _qpd = new QProgressDialog( tr("Mounting settings partition"), QString(), 0, 0, this);
    _qpd->setWindowModality(Qt::WindowModal);
    _qpd->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    _qpd->show();
    QApplication::processEvents();

    if (QProcess::execute("mount -t ext4 " SETTINGS_PARTITION " /settings") != 0)
    {
        _qpd->hide();

        if (QMessageBox::question(this,
                                  tr("Error mounting settings partition"),
                                  tr("Persistent settings partition seems corrupt. Reformat?"),
                                  QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
        {
            QProcess::execute("umount /settings");
            if (QProcess::execute("/sbin/mkfs.ext4 " SETTINGS_PARTITION) != 0
                || QProcess::execute("mount " SETTINGS_PARTITION " /settings") != 0)
            {
                QMessageBox::critical(this, tr("Reformat failed"), tr("SD card might be damaged"), QMessageBox::Close);
            }

            rebuildInstalledList();
        }
    }
    _qpd->hide();
    _qpd->deleteLater();
    _qpd = NULL;
    QProcess::execute("mount -o ro -t vfat /dev/mmcblk0p1 /mnt");
*/
    _model = getFileContents("/proc/device-tree/model");
    QString cmdline = getFileContents("/proc/cmdline");
    if (cmdline.contains("showall"))
    {
        _showAll = true;
    }
    /*
    if (cmdline.contains("silentinstall"))
    {
        _allowSilent = true;
    }
    else
    {
        startNetworking();
    }*/

    /* Disable online help buttons until network is functional */
    ui->actionBrowser->setEnabled(false);
    QTimer::singleShot(1, this, SLOT(populate()));
}

MainWindow::~MainWindow()
{
    QProcess::execute("umount " MEDIA_USB);
    QProcess::execute("umount " MEDIA_SDCARD);
    delete ui;
}

/* Discover which images we have, and fill in the list */
void MainWindow::populate()
{
    /* Ask user to wait while list is populated */
    if (!_allowSilent)
    {
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
    }

    connect(&_mediaPollTimer, SIGNAL(timeout()), SLOT(pollMedia()));
    _mediaPollTimer.start(100);


    _availableMB = (getFileContents("/sys/class/block/mmcblk0/size").trimmed().toULongLong()-getFileContents("/sys/class/block/mmcblk0p5/start").trimmed().toULongLong()-getFileContents("/sys/class/block/mmcblk0p5/size").trimmed().toULongLong())/2048;
    updateNeeded();

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
    bool haveicons = false;
    QSize currentsize = ui->list->iconSize();
    QIcon localIcon(":/icons/hdd.png");
    QIcon internetIcon(":/icons/download.png");

    foreach (QVariant v, images.values())
    {
        QVariantMap m = v.toMap();
        QString name = m.value("name").toString();
        QString description = m.value("description").toString();
        QString folder  = m.value("folder").toString();
        QString iconFilename = m.value("icon").toString();
        bool recommended = m.value("recommended").toBool();

        if (!iconFilename.isEmpty() && !iconFilename.contains(QDir::separator()))
            iconFilename = folder + QDir::separator() + iconFilename;

        QString friendlyname = name;
        if (recommended)
            friendlyname += " ["+tr("RECOMMENDED")+"]";
        if (!description.isEmpty())
            friendlyname += "\n"+description;

        QIcon icon;
        if (QFile::exists(iconFilename))
        {
            icon = QIcon(iconFilename);
            QList<QSize> avs = icon.availableSizes();
            if (avs.isEmpty())
            {
                /* Icon file corrupt */
                icon = QIcon();
            }
            else
            {
                QSize iconsize = avs.first();
                haveicons = true;

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
        item->setCheckState(Qt::Unchecked);

        if (folder.startsWith(QDir::separator()))
            item->setData(SecondIconRole, localIcon);
        else
            item->setData(SecondIconRole, internetIcon);

        if (recommended)
            ui->list->insertItem(0, item);
        else
            ui->list->addItem(item);
    }

    if (haveicons)
    {
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
    }

    ui->actionCancel->setEnabled(true);

    /* Hide progress dialog since we have images we could use now... */
    if (_qpd && images.count()) {
        _qpd->hide();
        _qpd->deleteLater();
        _qpd = NULL;
    }
}

/* Whether this OS should be displayed in the list of installable OSes */
bool MainWindow::canInstallOs(const QString &name, const QVariantMap &values)
{
    /* Can't simply pull "name" from "values" because in some JSON files it's "os_name" and in others it's "name" */

    return true;
    /* If it's not bootable, it isn't really an OS, so is always installable */
    if (!canBootOs(name, values))
    {
        return true;
    }

    /* Display OS in list if it is supported or "showall" is specified in recovery.cmdline */
    if (_showAll)
    {
        return true;
    }
    else
    {
        return isSupportedOs(name, values);
    }
}

/* Whether this OS is supported */
bool MainWindow::isSupportedOs(const QString &name, const QVariantMap &values)
{
    /* Can't simply pull "name" from "values" because in some JSON files it's "os_name" and in others it's "name" */

    return true;
    /* If it's not bootable, it isn't really an OS, so is always supported */
    if (!canBootOs(name, values))
    {
        return true;
    }

    if (values.contains("supported_models"))
    {
        QStringList supportedModels = values.value("supported_models").toStringList();

        foreach (QString m, supportedModels)
        {
            /* Check if the full formal model name (e.g. "Raspberry Pi 2 Model B Rev 1.1")
             * contains the string we are told to look for (e.g. "Pi 2") */
            if (_model.contains(m, Qt::CaseInsensitive))
            {
                return true;
            }
        }

        return false;
    }

    return true;
}

bool MainWindow::isMounted(const QString &path) {
    QFile file("/proc/mounts");
    qDebug() << path;
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

bool MainWindow::mountMedia(const QString &media, const QString &dst) {
    QDir().mkdir(dst);
    if (QProcess::execute("mount " + media + " " + dst) != 0)
    {
        qDebug() << "Error mounting external media" << media << "to" << dst;
        QMessageBox::critical(this, tr("Error mounting"), tr("Error mounting external media (%1)").arg(media), QMessageBox::Close);
        return false;
    }

    return true;
}

void MainWindow::pollMedia()
{
    static bool usbProcessed = false;
    static bool sdProcessed = false;

    if (!usbProcessed && QFile::exists(MEDIA_USB)) {
        if (_qpd) {
            _qpd->setLabelText(tr("Reading images from USB mass storage device..."));
            QApplication::processEvents();
        }

        QString dst = "/usb";
        if (!isMounted(MEDIA_USB)) {
            mountMedia(MEDIA_USB, dst);
        }

        QMap<QString,QVariantMap> images = listMediaImages(dst, SOURCE_USB);
        addImages(images);
        usbProcessed = true;
    }

    /* After first poll, set intervall to 2s */
    _mediaPollTimer.setInterval(2000);
}

QMap<QString, QVariantMap> MainWindow::listMediaImages(const QString &path, enum ImageSource source)
{
    QMap<QString,QVariantMap> images;

    /* Local image folders */
    QDir dir(path, "", QDir::Name, QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList list = dir.entryList();

    foreach(QString image, list) {
        QString imagefolder = path + QDir::separator() + image;
        qDebug() << imagefolder;
        QVariantMap imagemap = Json::loadFromFile(imagefolder + QDir::separator() + "image.json").toMap();

        QString basename = imagemap.value("name").toString();
        imagemap["recommended"] = true;
        imagemap["folder"] = imagefolder;
        imagemap["source"] = source;

        if (!imagemap.contains("nominal_size")) {
            // Calculate nominal_size based on partition information
            int nominal_size = 0;
            QVariantList pvl = imagemap.value("partitions").toList();

            foreach (QVariant v, pvl)
            {
                QVariantMap pv = v.toMap();
                nominal_size += pv.value("partition_size_nominal").toInt();
                nominal_size += 1; // Overhead per partition for EBR
            }
            imagemap["nominal_size"] = nominal_size;
        }

        imagemap["image_info"] = "image.json";
        images[basename] = imagemap;
    }

    return images;
}

void MainWindow::on_actionInstall_triggered()
{
    if (_silent ||
        QMessageBox::warning(this,
                            tr("Confirm"),
                            tr("Warning: this will install the selected Image. All existing data on the internal flash will be overwritten."),
                            QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
    {
        /* See if any of the OSes are unsupported */
        bool allSupported = true;
        QString unsupportedOses;
        QList<QListWidgetItem *> selected = selectedItems();
        foreach (QListWidgetItem *item, selected)
        {
            QVariantMap entry = item->data(Qt::UserRole).toMap();
            QString name = entry.value("name").toString();
            if (!isSupportedOs(name, entry))
            {
                allSupported = false;
                unsupportedOses += "\n" + name;
            }
        }
        if (_silent || allSupported ||
            QMessageBox::warning(this,
                                tr("Confirm"),
                                tr("Warning: incompatible Operating System(s) detected. The following OSes aren't supported on this revision of Raspberry Pi and may fail to boot or function correctly:") + unsupportedOses,
                                QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
        {
            setEnabled(false);
            _numMetaFilesToDownload = 0;

            QList<QListWidgetItem *> selected = selectedItems();
            foreach (QListWidgetItem *item, selected)
            {
                QVariantMap entry = item->data(Qt::UserRole).toMap();

                if (!entry.contains("folder"))
                {
                    QDir d;
                    QString folder = "/settings/os/"+entry.value("name").toString();
                    folder.replace(' ', '_');
                    if (!d.exists(folder))
                        d.mkpath(folder);

                    downloadMetaFile(entry.value("os_info").toString(), folder+"/os.json");
                    downloadMetaFile(entry.value("partitions_info").toString(), folder+"/partitions.json");

                    if (entry.contains("marketing_info"))
                        downloadMetaFile(entry.value("marketing_info").toString(), folder+"/marketing.tar");

                    if (entry.contains("partition_setup"))
                        downloadMetaFile(entry.value("partition_setup").toString(), folder+"/partition_setup.sh");

                    if (entry.contains("icon"))
                        downloadMetaFile(entry.value("icon").toString(), folder+"/icon.png");
                }
            }

            if (_numMetaFilesToDownload == 0)
            {
                /* All OSes selected are local */
                startImageWrite();
            }
            else if (!_silent)
            {
                _qpd = new QProgressDialog(tr("The install process will begin shortly."), QString(), 0, 0, this);
                _qpd->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
                _qpd->show();
            }
        }
    }
}

void MainWindow::on_actionCancel_triggered()
{
    close();
}

void MainWindow::onCompleted()
{
    _psd->hide();

    if (!_silent)
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

void MainWindow::on_list_currentRowChanged()
{
    QListWidgetItem *item = ui->list->currentItem();
    ui->actionEdit_config->setEnabled(item && item->data(Qt::UserRole).toMap().contains("partitions"));
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

void MainWindow::on_list_doubleClicked(const QModelIndex &index)
{
    if (index.isValid())
    {
        QListWidgetItem *item = ui->list->currentItem();
        if (item->checkState() == Qt::Unchecked)
            item->setCheckState(Qt::Checked);
        else
            item->setCheckState(Qt::Unchecked);
    }
}

void MainWindow::startNetworking()
{
    QFile f("/settings/wpa_supplicant.conf");

    if ( f.exists() && f.size() == 0 )
    {
        /* Remove corrupt file */
        f.remove();
    }
    if ( !f.exists() )
    {
        /* If user supplied a wpa_supplicant.conf on the FAT partition copy that one to settings
           otherwise copy the default one stored in the initramfs */
        if (QFile::exists("/mnt/wpa_supplicant.conf"))
            QFile::copy("/mnt/wpa_supplicant.conf", "/settings/wpa_supplicant.conf");
        else
        {
            qDebug() << "Copying /etc/wpa_supplicant.conf to /settings/wpa_supplicant.conf";
            QFile::copy("/etc/wpa_supplicant.conf", "/settings/wpa_supplicant.conf");
        }
    }
    QFile::remove("/etc/wpa_supplicant.conf");

    /* Enable dbus so that we can use it to talk to wpa_supplicant later */
    qDebug() << "Starting dbus";
    QProcess::execute("/etc/init.d/S30dbus start");

    /* Run dhcpcd in background */
    QProcess *proc = new QProcess(this);
    qDebug() << "Starting dhcpcd";
    proc->start("/sbin/dhcpcd --noarp -e wpa_supplicant_conf=/settings/wpa_supplicant.conf --denyinterfaces \"*_ap\"");

    _time.start();

    if ( isOnline() )
    {
        onOnlineStateChanged(true);
    }
    else
    {
        /* We could ask Qt's Bearer management to notify us once we are online,
           but it tends to poll every 10 seconds.
           Users are not that patient, so lets poll ourselves every 0.1 second */
        //QNetworkConfigurationManager *_netconfig = new QNetworkConfigurationManager(this);
        //connect(_netconfig, SIGNAL(onlineStateChanged(bool)), this, SLOT(onOnlineStateChanged(bool)));
        connect(&_networkStatusPollTimer, SIGNAL(timeout()), SLOT(pollNetworkStatus()));
        _networkStatusPollTimer.start(100);
    }
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
    qDebug() << "onOnlineStateChanged";
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
        ui->actionBrowser->setEnabled(true);
        emit networkUp();
    }
}

void MainWindow::downloadList(const QString &urlstring)
{
    QNetworkReply *reply = _netaccess->get(QNetworkRequest(QUrl(urlstring)));
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
        processJson(Json::parse( reply->readAll() ));
    }

    reply->deleteLater();
}

void MainWindow::processJson(QVariant json)
{
    if (json.isNull())
    {
        QMessageBox::critical(this, tr("Error"), tr("Error parsing list.json downloaded from server"), QMessageBox::Close);
        return;
    }

    QSet<QString> iconurls;
    QVariantList list = json.toMap().value("os_list").toList();

    foreach (QVariant osv, list)
    {
        QVariantMap  os = osv.toMap();

        QString basename = os.value("os_name").toString();
        if (canInstallOs(basename, os))
        {
            if (os.contains("flavours"))
            {
                QVariantList flavours = os.value("flavours").toList();

                foreach (QVariant flv, flavours)
                {
                    QVariantMap flavour = flv.toMap();
                    QVariantMap item = os;
                    QString name        = flavour.value("name").toString();
                    QString description = flavour.value("description").toString();
                    QString iconurl     = flavour.value("icon").toString();

                    item.insert("name", name);
                    item.insert("description", description);
                    item.insert("icon", iconurl);
                    item.insert("feature_level", flavour.value("feature_level"));
                    item.insert("source", SOURCE_NETWORK);

                    processJsonOs(name, item, iconurls);
                }
            }
            if (os.contains("description"))
            {
                QString name = basename;
                os["name"] = name;
                os["source"] = SOURCE_NETWORK;
                processJsonOs(name, os, iconurls);
            }
        }
    }

    /* Download icons */
    if (!iconurls.isEmpty())
    {
         _numIconsToDownload += iconurls.count();
        foreach (QString iconurl, iconurls)
        {
            downloadIcon(iconurl, iconurl);
        }
    }
    else
    {
        if (_qpd)
        {
            _qpd->deleteLater();
            _qpd = NULL;
        }
    }
}

void MainWindow::processJsonOs(const QString &name, QVariantMap &new_details, QSet<QString> &iconurls)
{
    QIcon internetIcon(":/icons/download.png");

    QListWidgetItem *witem = findItem(name);
    if (witem)
    {
        QVariantMap existing_details = witem->data(Qt::UserRole).toMap();
/*
        if ((existing_details["release_date"].toString() < new_details["release_date"].toString()) ||
                (existing_details["source"].toString() == SOURCE_INSTALLED_OS))
        {
            // Local version is older (or unavailable). Replace info with newer Internet version
            new_details.insert("installed", existing_details.value("installed", false));
            if (existing_details.contains("partitions"))
            {
                new_details["partitions"] = existing_details["partitions"];
            }
            witem->setData(Qt::UserRole, new_details);
            witem->setData(SecondIconRole, internetIcon);
            ui->list->update();
        }
*/
    }
    else
    {
        /* It's a new OS, so add it to the list */
        QString iconurl = new_details.value("icon").toString();
        QString description = new_details.value("description").toString();

        if (!iconurl.isEmpty())
            iconurls.insert(iconurl);

        bool recommended = (name == RECOMMENDED_IMAGE);

        QString friendlyname = name;
        if (recommended)
            friendlyname += " ["+tr("RECOMMENDED")+"]";
        if (!description.isEmpty())
            friendlyname += "\n"+description;

        witem = new QListWidgetItem(friendlyname);
        witem->setCheckState(Qt::Unchecked);
        witem->setData(Qt::UserRole, new_details);
        witem->setData(SecondIconRole, internetIcon);

        if (recommended)
            ui->list->insertItem(0, witem);
        else
            ui->list->addItem(witem);
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
            if (m.value("icon") == originalurl)
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
    QList<QListWidgetItem *> selected = selectedItems();

    foreach (QListWidgetItem *item, selected)
    {
        QVariantMap entry = item->data(Qt::UserRole).toMap();
        _neededMB += entry.value("nominal_size").toInt();
    }

    ui->neededLabel->setText(QString("%1: %2 MB").arg(tr("Needed"), QString::number(_neededMB)));
    ui->availableLabel->setText(QString("%1: %2 MB").arg(tr("Available"), QString::number(_availableMB)));

    if (_neededMB > _availableMB)
    {
        /* Selection exceeds available space, make label red to alert user */
        colorNeededLabel = Qt::red;
        bold = true;
    }
    else
    {
        /* Enable OK button if a selection has been made that fits on the card */
        enableOk = true;
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

void MainWindow::on_list_itemChanged(QListWidgetItem *)
{
    updateNeeded();
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
    else
        downloadListComplete();
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
        startImageWrite();
    }
}

void MainWindow::startImageWrite()
{
    /* All meta files downloaded, extract slides tarball, and launch image writer thread */
    MultiImageWriteThread *imageWriteThread = new MultiImageWriteThread();
    QString folder, slidesFolder;
    QStringList slidesFolders;

    /* Stop media poller in case still running */
    _mediaPollTimer.stop();

    QList<QListWidgetItem *> selected = selectedItems();
    foreach (QListWidgetItem *item, selected)
    {
        QVariantMap entry = item->data(Qt::UserRole).toMap();

        if (entry.contains("folder"))
        {
            /* Local image */
            folder = entry.value("folder").toString();
        }
        else
        {
            folder = "/settings/os/"+entry.value("name").toString();
            folder.replace(' ', '_');

            QString marketingTar = folder+"/marketing.tar";
            if (QFile::exists(marketingTar))
            {
                /* Extract tarball with slides */
                QProcess::execute("tar xf "+marketingTar+" -C "+folder);
                QFile::remove(marketingTar);
            }

            /* Insert tarball download URL information into partition_info.json */
            QVariantMap json = Json::loadFromFile(folder+"/partitions.json").toMap();
            QVariantList partitions = json["partitions"].toList();
            int i=0;
            QStringList tarballs = entry.value("tarballs").toStringList();
            foreach (QString tarball, tarballs)
            {
                QVariantMap partition = partitions[i].toMap();
                partition.insert("tarball", tarball);
                partitions[i] = partition;
                i++;
            }
            json["partitions"] = partitions;
            Json::saveToFile(folder+"/partitions.json", json);
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
        imageWriteThread->setImage(folder, entry.value("image_info").toString());
        if (!slidesFolder.isEmpty())
            slidesFolders.append(slidesFolder);


        // TODO: We only support one image. Option list instead of checkboxes?
        break;
    }

    if (slidesFolders.isEmpty())
        slidesFolder.append("/mnt/defaults/slides");

    _psd = new ProgressSlideshowDialog(slidesFolders, "", 20, this);
    connect(imageWriteThread, SIGNAL(parsedImagesize(qint64)), _psd, SLOT(setMaximum(qint64)));
    connect(imageWriteThread, SIGNAL(completed()), this, SLOT(onCompleted()));
    connect(imageWriteThread, SIGNAL(error(QString)), this, SLOT(onError(QString)));
    connect(imageWriteThread, SIGNAL(statusUpdate(QString)), _psd, SLOT(setLabelText(QString)));
    connect(imageWriteThread, SIGNAL(runningMKFS()), _psd, SLOT(pauseIOaccounting()), Qt::BlockingQueuedConnection);
    connect(imageWriteThread, SIGNAL(finishedMKFS()), _psd, SLOT(resumeIOaccounting()), Qt::BlockingQueuedConnection);
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
