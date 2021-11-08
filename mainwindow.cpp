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
#include "imagelistdownload.h"
#include "feedsdialog.h"
#include "waitingspinnerwidget.h"
#include "qlistimagewidgetitem.h"
#include "httpapi.h"
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


/* Main window
 *
 * Initial author: Floris Bos
 * Maintained by Raspberry Pi
 *
 * See LICENSE.txt for license details
 *
 */

MainWindow::MainWindow(LanguageDialog* ld, bool allowAutoinstall, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow), _fileSystemWatcher(new QFileSystemWatcher), _fileSystemWatcherFb(new QFileSystemWatcher),
    _qpd(nullptr), _psd(nullptr), _allowAutoinstall(allowAutoinstall), _isAutoinstall(false), _showAll(false),
    _ld(ld), _wasOnNetwork(false), _wasRndis(false), _downloadNetwork(true), _downloadRndis(true),
    _imageListDownloadsActive(0), _netaccess(nullptr), _imageWriteThread(nullptr), _internetHostLookupId(-1),
    _browser(new ZConfServiceBrowser), _httpApi(new HttpApi), _TeziState(TEZI_IDLE), _acceptAllLicenses(false)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowState(Qt::WindowMaximized);
    setContextMenuPolicy(Qt::NoContextMenu);
    ui->setupUi(this);
}

bool MainWindow::writeBootConfigurationBlock() {
    QStringList kobsngargs;
    QByteArray output;

    /* Start kobs verbose and let it write no boot loader (Tezi image will flash the bootloader later) */
    kobsngargs << "init" << "-v" << "-w" << "/dev/null";

    if (MultiImageWriteThread::runCommand("kobs-ng", kobsngargs, output, 10000))
    {
        qDebug() << "Kobs-ng output:";
        qDebug() << output;
        return true;
    }

    return false;
}

bool MainWindow::setAvahiHostname(const QString hostname) {
    QStringList avahiArgs;
    QByteArray output;

    avahiArgs << hostname;

    if (MultiImageWriteThread::runCommand("/usr/bin/avahi-set-host-name", avahiArgs, output, 10000))
    {
        qDebug() << "avahi-set-host-name:";
        qDebug() << output;
        return true;
    }

    return false;
}

bool MainWindow::initialize() {
    _usbGadget = new UsbGadget();
    _usbGadget->initRndis();
    _usbGadget->enableRndis(true);

    _moduleInformation = ModuleInformation::detectModule(this);
    if (_moduleInformation == nullptr) {
        QMessageBox::critical(nullptr, QObject::tr("Module Detection failed"),
                              QObject::tr("Failed to detect the basic module type. Cannot continue."),
                              QMessageBox::Close);
        return false;
    }

    if (!_moduleInformation->moduleSupported()) {
        QMessageBox::critical(nullptr, QObject::tr("Unsupported Module Type"),
                              QObject::tr("This module is an early sample which is not supported by the Toradex Easy Installer due to chip issues."),
                              QMessageBox::Close);
        return false;
    }

#ifdef __x86_64__
    _toradexConfigBlock = ConfigBlock::configBlockFromUserInput(27, "V1.1A", "01234567");
#else
    _toradexConfigBlock = _moduleInformation->readConfigBlock();
#endif //__x86_64__

    // Unlock by default
    _moduleInformation->unlockFlash();

    // No config block found, ask the user to create a new one using Label information
    if (_toradexConfigBlock == nullptr) {
        if (_moduleInformation->storageClass() == ModuleInformation::StorageClass::Mtd) {
            // We have to assume that we need to rebuild the boot ROM specific boot configuration block (BCB/FCB)
            if (!writeBootConfigurationBlock())
                QMessageBox::critical(nullptr, QObject::tr("Restoring Boot Configuration Block failed"),
                                      QObject::tr("Restoring Boot Configuration Block failed. Please check the logfile in /var/volatile/tezi.log and contact the Toradex support."),
                                      QMessageBox::Ok);
        }

        QMessageBox::critical(nullptr, QObject::tr("Reading Config Block failed"),
                              QObject::tr("Reading the Toradex Config Block failed, the Toradex Config Block might be erased or corrupted. "
                                          "Please restore the Config Block using the information on the Modules Sticker before continuing."),
                              QMessageBox::Ok);

        ConfigBlockDialog* cbd = new ConfigBlockDialog(_moduleInformation->productIds(), this);
        if (cbd->exec() == QDialog::Accepted) {
            // The user created a new config block...
            _toradexConfigBlock = cbd->configBlock;
        } else {
            QApplication::exit(LINUX_POWEROFF);
        }
    }

    /* Write Config Block if needed */
    _moduleInformation->writeConfigBlockIfNeeded(_toradexConfigBlock);

    updateVersion();
    _toradexProductName = _toradexConfigBlock->getProductName();
    _toradexBoardRev = _toradexConfigBlock->getBoardRev();
    _serialNumber = _toradexConfigBlock->getSerialNumber();
    _toradexProductId = _toradexConfigBlock->getProductId();
    _toradexProductNumber = _toradexConfigBlock->getProductNumber();
    setAvahiHostname(QString("%1-%2").arg(_moduleInformation->getHostname(), _serialNumber));
    _toradexPID8 = _toradexConfigBlock->getPID8();
    updateModuleInformation();

    ui->list->setItemDelegate(new TwoIconsDelegate(this, ui->list));
    ui->list->setIconSize(QSize(32, 32));

    QString cmdline = getFileContents("/proc/cmdline");
    if (cmdline.contains("showall"))
    {
        _showAll = true;
    }

    /* Initialize icons */
    _sdIcon = QIcon(":/icons/sd_memory.png");
    _usbIcon = QIcon(":/icons/flashdisk_logo.png");
    _networkIcon = QIcon(":/icons/download.png");
    _internetIcon = QIcon(":/icons/download_cloud.png");

    _mediaPollThread = new MediaPollThread(_moduleInformation, this);

    ui->list->setSelectionMode(QAbstractItemView::SingleSelection);

    _availableMB = static_cast<int>(_moduleInformation->getStorageSize() / (1024 * 1024));

    if (_moduleInformation->storageClass() == ModuleInformation::StorageClass::Block) {
        _usbGadget->setMassStorageDev(_moduleInformation->mainPartition());
        if (_usbGadget->initMassStorage() &&
            _usbGadget->setMassStorageAttrs(_serialNumber, _toradexProductName, _toradexProductId))
            ui->actionUsbMassStorage->setEnabled(true);
        else
            ui->actionUsbMassStorage->setEnabled(false);
    } else {
        ui->mainToolBar->removeAction(ui->actionUsbMassStorage);
    }

    if (_usbGadget->initRndis() &&
        _usbGadget->setRndisAttrs(_serialNumber, _toradexProductName, _toradexProductId)) {
        ui->actionUsbRndis->setEnabled(true);
        ui->actionUsbRndis->trigger();

    } else {
        ui->actionUsbRndis->setEnabled(false);
    }

    // Add static server list
    FeedServer srv;
    srv.label = "Toradex Image Server";
    srv.url = DEFAULT_IMAGE_SERVER;
    srv.url.append("?PID8=").append(_toradexPID8);
    srv.source = SOURCE_INTERNET;
    srv.enabled = true;
    _networkFeedServerList.append(srv);

    srv.label = "Toradex 3rd Party Image Server";
    srv.url = DEFAULT_3RDPARTY_IMAGE_SERVER;
    srv.url.append("?PID8=").append(_toradexPID8);
    srv.source = SOURCE_INTERNET;
    srv.enabled = true;
    _networkFeedServerList.append(srv);

    srv.label = "Toradex Continuous Integration Server (testing)";
    srv.url = DEFAULT_CI_IMAGE_SERVER;
    srv.url.append("?PID8=").append(_toradexPID8);
    srv.source = SOURCE_INTERNET;
    srv.enabled = false;
    _networkFeedServerList.append(srv);

    qRegisterMetaType<QListVariantMap>("QListVariantMap");

    // Image list contains and maintains all currently downloaded image descriptions (image.json)
    _imageList = new ImageList(_toradexPID8);
    connect(_imageList, SIGNAL (imageListUpdated()), this, SLOT (imageListUpdated()));
    connect(_imageList, SIGNAL (installImage(const QVariantMap, const bool)), this, SLOT (installImage(const QVariantMap, const bool)));
    // Starting network after first media scan makes sure that a local media takes presedence over a server provided image.
    connect(_mediaPollThread, SIGNAL (firstScanFinished()), this, SLOT (startNetworking()));
    connect(_mediaPollThread, SIGNAL (removedBlockdev(const QString)), _imageList, SLOT (removeImagesByBlockdev(const QString)));
    connect(_mediaPollThread, SIGNAL (newImageUrl(const QString)), this, SLOT (addNewImageUrl(const QString)));
    connect(_mediaPollThread, SIGNAL (newImagesToAdd(QListVariantMap)), _imageList, SLOT (addImages(QListVariantMap)));
    connect(_mediaPollThread, SIGNAL (errorMounting(const QString)), this, SLOT (errorMounting(const QString)));
    connect(_mediaPollThread, SIGNAL (disableFeed(const QString)), this, SLOT (disableFeed(const QString)));
    connect(_httpApi, SIGNAL (httpApiInstallImageById(const QVariantMap)), this, SLOT (downloadImage(const QVariantMap)));
    connect(_httpApi, SIGNAL (httpApiInstallImageByUrl(const QString, enum ImageSource)), this, SLOT (downloadImage(const QString, enum ImageSource)));
    connect(_browser, SIGNAL (serviceEntryAdded(QString)), this, SLOT (addService(QString)));
    connect(_browser, SIGNAL (serviceEntryRemoved(QString)), this, SLOT (removeService(QString)));
    connect(_httpApi, SIGNAL (newImageUrl(const QString)), this, SLOT (addNewImageUrl(const QString)));

    _browser->browse("_tezi._tcp");
    _httpApi->start("8080",this);
    _mediaPollThread->start();

    return true;
}

MainWindow::~MainWindow()
{
    QProcess::execute("umount " SRC_MOUNT_FOLDER);
    delete ui;
}

void MainWindow::setWorkingInBackground(bool working, const QString &labelText)
{
    if (working) {
        _TeziState = TEZI_SCANNING;
        ui->waitingSpinner->start();
        ui->labelBackgroundTask->setText(labelText);
    } else {
        if (_TeziState == TEZI_SCANNING)
            _TeziState = TEZI_IDLE;
        ui->waitingSpinner->stop();
        ui->labelBackgroundTask->setText("");
    }
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
    setWorkingInBackground(true, tr("Waiting for external media or network..."));
}

void MainWindow::showProgressDialog(const QString &labelText)
{
    /* Ask user to wait while list is populated */
    _qpd = new QProgressDialog(labelText, QString(), 0, 0, this);
    _qpd->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    _qpd->setModal(true);
    _qpd->show();
}

void MainWindow::imageListUpdated()
{
    QSize currentsize = ui->list->iconSize();
    QPixmap dummyicon = QPixmap(currentsize.width(), currentsize.height());
    dummyicon.fill();

    for (int i = 0; i < ui->list->count(); i++)
    {
        QListWidgetItem *item = ui->list->item(i);
        delete item;
        i--;
    }
    foreach (const QVariantMap &m, _imageList->imageList())
    {
        QString name = m.value("name").toString();
        QString description = m.value("description").toString();
        QString version = m.value("version").toString();
        QString releasedate = m.value("release_date").toString();
        QByteArray icondata = m.value("iconimage").value<QByteArray>();
        bool supportedImage = m.value("supported_image").toBool();
        bool supportedConfigFormat = m.value("supported_config_format").toBool();
        enum ImageSource source = m.value("source").value<enum ImageSource>();
        QString imageURI = m.value("uri").toString();

        QString imageName = name;
        QString imageInfo, imageVersion, space(" "), eol("\n");
        imageInfo += description;
        if (!supportedImage)
            imageVersion += " [" + tr("image not compatible with this module") + "] ";
        else if (!supportedConfigFormat)
            imageVersion += " [" + tr("image requires a newer version of the installer") + "] ";

        if (!version.isEmpty())
            imageVersion += version + space;
        else
            imageVersion += "Unknown version" + space;

        if (!releasedate.isEmpty())
            imageVersion += "(" + releasedate + ")" + eol;

        QPixmap pix;
        pix.loadFromData(icondata);
        QIcon icon(pix);
        if (!icon.isNull()) {
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
                }
            }
        }

        QListImageWidgetItem *item = new QListImageWidgetItem(icon, imageName);

        item->setData(Qt::UserRole, m);
        if (!icon.isNull())
            item->setData(Qt::DecorationRole, icon);
        else
            item->setData(Qt::DecorationRole, dummyicon);
        item->setData(NameRole, name);
        item->setData(VersionRole, imageVersion);
        item->setData(InfoRole, imageInfo);
        item->setData(URIRole, imageURI);
        item->setToolTip("<nobr>" + name + "</nobr>" + "<br>" + imageVersion + "<br>" + imageInfo + "<br>" + imageURI);

        if (supportedImage && supportedConfigFormat)
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        else
            item->setFlags(Qt::NoItemFlags);

        if (source == SOURCE_USB)
            item->setData(SecondIconRole, _usbIcon);
        else if (source == SOURCE_SDCARD)
            item->setData(SecondIconRole, _sdIcon);
        else if (source == SOURCE_INTERNET)
            item->setData(SecondIconRole, _internetIcon);
        else if (source == SOURCE_NETWORK || source == SOURCE_RNDIS)
            item->setData(SecondIconRole, _networkIcon);

        ui->list->addItem(item);
    }

    /* Remove waiting spinner as soon as there is something the user might want to look at... */
    if (ui->list->count() > 0) {
        if (_imageListDownloadsActive == 0)
            setWorkingInBackground(false);
    } else {
        setWorkingInBackground(true, tr("Waiting for external media or network..."));
    }
    updateNeeded();
}

void MainWindow::addNewImageUrl(const QString url)
{
    FeedServer server;
    QMutexLocker locker(&feedMutex);
    server.label = tr("Custom Server from Media");
    server.url = url;

    // Classify as network by default. This could be a public or
    // a local url, we can't tell...
    if (url.contains(RNDIS_ADDRESS))
        server.source = SOURCE_RNDIS;
    else
        server.source = SOURCE_NETWORK;
    server.enabled = true;

    int index = _networkFeedServerList.indexOf(server);
    if (index < 0) {
        _networkFeedServerList.append(server);
    } else {
        /*
         * Enable if not yet enabled and refresh list
         * This allows to force enable a statically added list
         * using an external media
         */
        if (_networkFeedServerList[index].enabled)
            return;
        else
            _networkFeedServerList[index].enabled = true;
    }
    _imageList->removeImagesBySource(server.source);
    if (server.source == SOURCE_RNDIS)
        _downloadRndis = true;
    else if (server.source == SOURCE_NETWORK)
        _downloadNetwork = true;
}

void MainWindow::on_list_currentItemChanged()
{
    updateNeeded();
}

void MainWindow::on_list_itemDoubleClicked()
{
    if (!ui->actionInstall->isEnabled())
        return;

    on_actionInstall_triggered();
}

void MainWindow::errorMounting(const QString blockdev)
{
    QMessageBox::critical(this, tr("Error mounting"),
                          tr("Error mounting external device (%1)").arg(blockdev),
                          QMessageBox::Close);
}

void MainWindow::disableFeed(const QString feedname)
{
        QMutexLocker locker(&feedMutex);
        if (feedname == TEZI_CONFIG_JSON_DEFAULT_FEED)
            _networkFeedServerList[0].enabled = false;
        else if (feedname == TEZI_CONFIG_JSON_3RDPARTY_FEED)
            _networkFeedServerList[1].enabled = false;
}

bool MainWindow::validateImageJson(QVariantMap &entry)
{
    /* Validate image */
    QString validationerror;
    QFile schemafile(":/tezi.schema");
    schemafile.open(QIODevice::ReadOnly);
    QByteArray schemadata = schemafile.readAll();
    schemafile.close();

    QFile jsonfile(entry.value("folder").value<QString>() + QDir::separator() + entry.value("image_info").value<QString>());
    jsonfile.open(QIODevice::ReadOnly);
    QByteArray jsondata = jsonfile.readAll();
    jsonfile.close();

    if (!Json::validate(schemadata, jsondata, validationerror)) {
        QMessageBox::critical(this, tr("Error"), tr("Image JSON validation failed for '%1'").arg(entry.value("name").value<QString>()) + "\n\n"
                              + validationerror, QMessageBox::Close);
        qDebug() << "Image JSON validation failed for" << entry.value("name").value<QString>();
        qDebug() << validationerror;
        return false;
    }
    return true;
}

void MainWindow::installImage(const QVariantMap &image, const bool isAutoInstall)
{
    if (!_allowAutoinstall && isAutoInstall)
        return;

    _isAutoinstall = isAutoInstall;
    installImage(image);
}

void MainWindow::installImage(QVariantMap entry)
{
    enum ImageSource imageSource = entry.value("source").value<enum ImageSource>();

    _numMetaFilesToDownload = 0;
    _moduleInformation->unlockFlash();
    _installingFromMedia = !ImageInfo::isNetwork(imageSource);

    disableImageChoice();

    /* Re-mount local media */
    if (_installingFromMedia) {
        QString blockdev = entry.value("image_source_blockdev").value<QString>();
        if (!_mediaPollThread->mountMedia(blockdev)) {
            errorMounting(blockdev);
            clearInstallSettings();
            reenableImageChoice();
            _TeziState = TEZI_FAILED;
            return;
        }
    }

    if (entry.value("validate", true).value<bool>()) {
        if (!validateImageJson(entry)) {
            clearInstallSettings();
            reenableImageChoice();
            _TeziState = TEZI_FAILED;
            return;
        }
    }

    _TeziState = TEZI_INSTALLING;
    emit abortAllDownloads();

    if (ImageInfo::isNetwork(imageSource))
    {
        _imageEntry = entry;

        QString folder = entry.value("folder").value<QString>();
        QString url = entry.value("baseurl").value<QString>();

        if (entry.contains("marketing"))
            downloadMetaFile(url + entry.value("marketing").toString(), folder + "/marketing.tar");

        if (entry.contains("prepare_script")) {
            QString script = entry.value("prepare_script").toString();
            downloadMetaFile(url + script, folder + "/" + script);
        }
        if (entry.contains("wrapup_script")) {
            QString script = entry.value("wrapup_script").toString();
            downloadMetaFile(url + script, folder + "/" + script);
        }
        if (entry.contains("error_script")) {
            QString script = entry.value("error_script").toString();
            downloadMetaFile(url + script, folder + "/" + script);
        }
        if (entry.contains("u_boot_env")) {
            QString envtxt = entry.value("u_boot_env").toString();
            downloadMetaFile(url + envtxt, folder + "/" + envtxt);
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

    /* If asynchrounous meta data download is in progress, wait for them to finish... */
    if (_numMetaFilesToDownload == 0)
        startImageWrite(entry);
    else
        showProgressDialog(tr("The installation process will begin shortly."));
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
    if (_moduleInformation->storageClass() == ModuleInformation::StorageClass::Block)
        ui->actionUsbMassStorage->setEnabled(!checked);
}

void MainWindow::on_actionEraseModule_triggered()
{
    switch (_moduleInformation->storageClass()) {
    case ModuleInformation::StorageClass::Block:
        discardBlockdev();
        break;
    case ModuleInformation::StorageClass::Mtd:
        eraseMtd();
        break;
    }
}

void MainWindow::eraseMtd()
{
    if (QMessageBox::warning(this,
                            tr("Confirm"),
                            tr("This erases all data on the internal raw NAND flash, including boot loader and boot loader configuration as well as wear leveling information. "
                               "After this operation you either need to install an image or use the module's recovery mode to boot back into Toradex Easy Installer. Continue?"),
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
    {
        showProgressDialog(tr("Erasing internal raw NAND..."));
        MtdEraseThread *thread = new MtdEraseThread(_moduleInformation->erasePartitions());

        connect(thread, SIGNAL (finished()), this, SLOT (discardOrEraseFinished()));
        connect(thread, SIGNAL (error(QString)), this, SLOT (discardOrEraseError(QString)));

        thread->start();
    }
}

void MainWindow::discardBlockdev()
{
    if (QMessageBox::warning(this,
                            tr("Confirm"),
                            tr("This discards all data on the internal eMMC, including boot loader and boot loader configuration. "
                               "After this operation you either need to install an image or use the module's recovery mode to boot back into Toradex Easy Installer. Continue?"),
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
    {
        _moduleInformation->unlockFlash();

        showProgressDialog(tr("Discarding all data on internal eMMC..."));
        /*
         * Note: In the eMMC case we discard all partitons. It is not easy to carve out
         * small portitions of blocks since discard seems to operate on erase block size.
         * So even when specifiying "discard 0 - (end - 512 bytes), the eMMC actually
         * discards the last 512 bytes...
         *
         * On eMMC its easy to just restore the config block which we read on startup...
         */
        DiscardThread *thread = new DiscardThread(_moduleInformation->erasePartitions());
        _toradexConfigBlock->needsWrite = true;

        connect(thread, SIGNAL (finished()), this, SLOT (discardOrEraseFinished()));
        connect(thread, SIGNAL (error(QString)), this, SLOT (discardOrEraseError(QString)));

        thread->start();
    }
}

void MainWindow::discardOrEraseFinished()
{
    DiscardThread *thread = qobject_cast<DiscardThread *>(sender());

    /* In the eMMC case we have to restore config block... */
    if (_toradexConfigBlock->needsWrite) {
        _toradexConfigBlock->writeToBlockdev(_moduleInformation->configBlockPartition(), _moduleInformation->configBlockOffset());
        _toradexConfigBlock->needsWrite = false;
    }

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

void MainWindow::on_actionEditFeeds_triggered()
{
    FeedsDialog feeds(_networkFeedServerList, this);
    if (feeds.exec() == QDialog::Accepted)
        on_actionRefreshCloud_triggered();
}

void MainWindow::on_actionInstall_triggered()
{
    if (!ui->list->currentItem())
        return;

    QListWidgetItem *item = ui->list->currentItem();
    QVariantMap entry = item->data(Qt::UserRole).toMap();

    if (QMessageBox::warning(this,
                            tr("Confirm"),
                            tr("Warning: this will install the selected Image. All existing data on the internal flash will be overwritten."),
                            QMessageBox::Yes, QMessageBox::No) == QMessageBox::Yes)
    {
        _acceptAllLicenses = false;
        installImage(entry);
    }
}

void MainWindow::on_actionRefreshCloud_triggered()
{
    _imageList->removeImagesBySource(SOURCE_NETWORK);
    _imageList->removeImagesBySource(SOURCE_INTERNET);
    downloadLists(SOURCE_NETWORK);

    _imageList->removeImagesBySource(SOURCE_RNDIS);
    downloadLists(SOURCE_RNDIS);

    /* Try to lookup image host before trying to download images from the Internet */
    _internetHostLookupId = QHostInfo::lookupHost(DEFAULT_IMAGE_HOST,
                          this, SLOT(imageHostLookupResults(QHostInfo)));
}

void MainWindow::on_actionCancel_triggered()
{
    QMessageBox msgbox(QMessageBox::Information, tr("Exit Toradex Easy Installer"),
                       tr("Do you want to power off or reboot the module?"),
                       QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, this);
    msgbox.button(QMessageBox::Yes)->setText(tr("Power off"));
    msgbox.button(QMessageBox::No)->setText(tr("Reboot"));
    msgbox.button(QMessageBox::Cancel)->setText(tr("Return to menu"));

    int value = msgbox.exec();

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
        break;
    }
}

void MainWindow::onCompleted()
{
    /* Directly reboot into newer Toradex Easy Installer */
    if (_imageWriteThread->getImageInfo()->isInstaller() && _isAutoinstall) {
        close();
        /* A case for kexec... Anyone? :-) */
        QApplication::exit(LINUX_REBOOT);
    }

    clearInstallSettings();
    _TeziState = TEZI_INSTALLED;

    QString text = tr("The Image has been installed successfully.") + " <b>" + tr("You can now safely power off or reset the system.") + "</b>";

    if (!_moduleInformation->rebootWorks())
        text += "<br><br>" + tr("In case recovery mode has been used a power cycle will be necessary.");

    QMessageBox msgbox(QMessageBox::Information, tr("Image Installed"), text,
                       QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, this);
    msgbox.button(QMessageBox::Yes)->setText(tr("Power off"));
    msgbox.button(QMessageBox::No)->setText(tr("Reboot"));
    msgbox.button(QMessageBox::Cancel)->setText(tr("Return to menu"));

    int value = msgbox.exec();

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
        if (_ld != nullptr)
            _ld->show();
        break;
    }
}

void MainWindow::onError(const QString &msg)
{
    qDebug() << "Error:" << msg;

    QString errMsg = tr("The image has not been written completely. Please restart the process, otherwise you might end up in a non-bootable system.\n");
    QString errorScriptFilename = _imageWriteThread->getImageInfo()->errorScript();
    QString errorScript =_imageWriteThread->getImageInfo()->folder() +
            "/" + errorScriptFilename;
    QByteArray output;
    if (!errorScriptFilename.isEmpty()) {
        if (_imageWriteThread->runScript(errorScript, output))
            errMsg += tr("\nThe error script ") + errorScript + tr(" was executed.");
        else
            errMsg += tr("\nError executing error script ") + errorScript + ": " + tr(output);
    } else
        qDebug() << "No error script found";

    QMessageBox::critical(this, tr("Error"), msg  + "\n\n" + errMsg, QMessageBox::Ok);

     _TeziState = TEZI_FAILED;

    clearInstallSettings();
    reenableImageChoice();
    QWidget::show();
    if (_ld != nullptr)
        _ld->show();
}

void MainWindow::onQuery(const QString &msg, const QString &title, QMessageBox::StandardButton* answer)
{
    *answer = QMessageBox::question(this, title, msg, QMessageBox::Yes|QMessageBox::No);
}

void MainWindow::addService(QString service)
{
    QMutexLocker locker(&feedMutex);
    ZConfServiceEntry serviceEntry = _browser->serviceEntry(service);
    if (!serviceEntry.isValid())
        return;

    if (!serviceEntry.TXTRecords.contains("path"))
        return;


    FeedServer srv;
    srv.label = service + " (zeroconf)";
    QString protocol = "http://";
    if (serviceEntry.TXTRecords.value("https", "0").toInt() == 1)
        protocol = "https://";

    QString port = "";
    if (serviceEntry.port)
        port = QString(":%1").arg(serviceEntry.port);

    srv.url = protocol + serviceEntry.host + port + serviceEntry.TXTRecords.value("path");
    srv.source = SOURCE_NETWORK;
    srv.enabled = serviceEntry.TXTRecords.value("enabled", "1").toInt();

    /* Only add the service once */
    int index = _networkFeedServerList.indexOf(srv);
    if (index < 0) {
        qDebug() << "Adding zeroconf feed" << service;
        _networkFeedServerList.append(srv);
        _downloadNetwork = true;
    }
}

void MainWindow::removeService(QString service)
{
    int idx = -1;
    QMutexLocker locker(&feedMutex);
    for (int i = 0; i < _networkFeedServerList.count(); i++)
    {
        if (_networkFeedServerList[i].label == service + " (zeroconf)")
            idx = i;
    }
    if (idx > 0)
    {
        qDebug() << "Removing zeroconf feed" << service;
        _networkFeedServerList.removeAt(idx);
        _downloadNetwork = true;
    }
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

void MainWindow::on_actionEdit_config_triggered()
{
}

void MainWindow::startNetworking()
{
    _elapsedTimer.start();

    // Poll network interface changes every 100ms
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

        /* Check whether this is an address of global scope and a IPv4 address */
        if (a != QHostAddress::LocalHost && a != QHostAddress::LocalHostIPv6 &&
            a.scopeId() == "" && a.protocol() == QAbstractSocket::IPv4Protocol) {
            if (currAddress != nullptr)
                *currAddress = ae;
            return true;
        }
    }

    return false;
}

void MainWindow::pollNetworkStatus()
{
    QNetworkAddressEntry ae;
    if (hasAddress("eth0", &ae) || hasAddress("eth1", &ae)) {
        if (!_wasOnNetwork) {
            qDebug() << "Network up in" << _elapsedTimer.elapsed()/1000.0 << "seconds";
            ui->labelEthernetAddress->setText(
                        QString("%2/%3").arg(ae.ip().toString(), QString::number(ae.prefixLength())));

            setWorkingInBackground(true, tr("Downloading image list ..."));

            /* Check for Internet connectivity */
            _internetHostLookupId = QHostInfo::lookupHost(DEFAULT_IMAGE_HOST,
                                  this, SLOT(imageHostLookupResults(QHostInfo)));

            _wasOnNetwork = true;
        }

        if (_downloadNetwork) {
            _downloadNetwork = false;
            _imageList->removeImagesBySource(SOURCE_NETWORK);
            downloadLists(SOURCE_NETWORK);
        }
    } else {
        if (_wasOnNetwork) {
            ui->labelEthernetAddress->setText(tr("No address assigned"));
            _imageList->removeImagesBySource(SOURCE_NETWORK);
            _imageList->removeImagesBySource(SOURCE_INTERNET);
            _wasOnNetwork = false;
            _downloadNetwork = true;
        }
    }

    if (hasAddress("usb0", &ae)) {

        if (!_wasRndis) {
            qDebug() << "RNDIS up in" << _elapsedTimer.elapsed()/1000.0 << "seconds";
            ui->labelRNDISAddress->setText(
                        QString("%2/%3").arg(ae.ip().toString(), QString::number(ae.prefixLength())));

            _wasRndis = true;
        }

        if (_downloadRndis) {
            _downloadRndis = false;
            _imageList->removeImagesBySource(SOURCE_RNDIS);
            downloadLists(SOURCE_RNDIS);
        }

    } else {
        if (_wasRndis) {
            ui->labelRNDISAddress->setText(tr("No address assigned"));
            _imageList->removeImagesBySource(SOURCE_RNDIS);
            _wasRndis = false;
            _downloadRndis = true;
        }
    }
}

void MainWindow::imageHostLookupResults(QHostInfo hostInfo){
    /* Name server lookup result */
    if (hostInfo.error() != QHostInfo::NoError) {
        qDebug() << hostInfo.errorString();

        _internetHostLookupId = QHostInfo::lookupHost(DEFAULT_IMAGE_HOST,
                              this, SLOT(imageHostLookupResults(QHostInfo)));
    } else {
        qDebug() << "Tezi Server lookup successfully";
        _internetHostLookupId = -1;
        _imageList->removeImagesBySource(SOURCE_INTERNET);
        downloadLists(SOURCE_INTERNET);
    }
}

void MainWindow::downloadImageSetupSignals(ImageListDownload *imageListDownload)
{
    connect(imageListDownload, SIGNAL(newImagesToAdd(QListVariantMap)), _imageList, SLOT(addImages(QListVariantMap)));
    connect(imageListDownload, SIGNAL(finished()), this, SLOT(onImageListDownloadFinished()));
    connect(imageListDownload, SIGNAL(error(QString)), this, SLOT(onImageListDownloadError(QString)));
    connect(imageListDownload, SIGNAL(installImage(const QVariantMap, const bool)), this, SLOT(installImage(const QVariantMap, const bool)));
    _imageListDownloadsActive++;
}

void MainWindow::downloadImageList(const QString &url, enum ImageSource source, int index)
{
    ImageListDownload *imageListDownload = new ImageListDownload(url, source, index, _netaccess, this);
    downloadImageSetupSignals(imageListDownload);
}

void MainWindow::downloadImage(const QVariantMap image)
{
    installImage(image);
}

void MainWindow::downloadImage(const QString &url, enum ImageSource source)
{
    ImageListDownload *imageListDownload = new ImageListDownload(url, source, _netaccess, this);
    downloadImageSetupSignals(imageListDownload);
}

bool MainWindow::downloadLists(const enum ImageSource source)
{
    int index = 0;

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

    QMutexLocker locker(&feedMutex);
    foreach (FeedServer server, _networkFeedServerList)
    {
        if (server.source != source)
            continue;

        if (!server.enabled)
            continue;

        downloadImageList(server.url, server.source, index);

        index++;
    }

    if (_imageListDownloadsActive > 0)
    {
        setWorkingInBackground(true, tr("Reloading images from network..."));
        this->ui->actionRefreshCloud->setDisabled(true);
    }
    return _imageListDownloadsActive > 0;
}

void MainWindow::onImageListDownloadFinished()
{
    if (_imageListDownloadsActive)
        _imageListDownloadsActive--;

    if (_imageListDownloadsActive == 0)
        setWorkingInBackground(false);
    this->ui->actionRefreshCloud->setDisabled(false);
}

void MainWindow::onImageListDownloadError(const QString &msg)
{
    qDebug() << "Error:" << msg;

    QMessageBox::critical(this, tr("Error"), msg, QMessageBox::Ok);
}

QListWidgetItem *MainWindow::findItem(const QVariant &name)
{
    for (int i = 0; i < ui->list->count(); i++)
    {
        QListWidgetItem *item = ui->list->item(i);
        QVariantMap m = item->data(Qt::UserRole).toMap();
        if (m.value("name").toString() == name.toString())
        {
            return item;
        }
    }
    return nullptr;
}

QList<QListWidgetItem *> MainWindow::selectedItems()
{
    QList<QListWidgetItem *> selected;

    for (int i = 0; i < ui->list->count(); i++)
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
        clearInstallSettings();
        reenableImageChoice();
        _TeziState = TEZI_FAILED;
    } else {
        _numMetaFilesToDownload--;
    }
    fd->deleteLater();

    if (_numMetaFilesToDownload == 0) {
        if (_qpd)
        {
            _qpd->hide();
            _qpd->deleteLater();
            _qpd = nullptr;
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
        _qpd = nullptr;
    }
    QMessageBox::critical(this, tr("Download error"), tr("Error downloading meta file")+"\n"+fd->urlString(), QMessageBox::Close);
    clearInstallSettings();
    reenableImageChoice();

    _TeziState = TEZI_FAILED;
    fd->deleteLater();
}

void MainWindow::clearInstallSettings()
{
    if (_installingFromMedia)
        _mediaPollThread->unmountMedia();

    if (_psd != nullptr) {
        _psd->close();
        _psd->deleteLater();
        _psd = nullptr;
    }

    if (_imageWriteThread != nullptr) {
        _imageWriteThread->deleteLater();
        _imageWriteThread = nullptr;
    }

    _isAutoinstall = false;
    _acceptAllLicenses = false;
}

/*
 * Disable timers and media poll thread
 *
 * Must be called after mounting the image media, otherwise
 * there is a deadlock possibility scanMutex vs. mountMutex.
 */
void MainWindow::disableImageChoice()
{
    _networkStatusPollTimer.stop();
    _mediaPollThread->scanMutex.lock();
    setEnabled(false);
}

/*
 * Returning to main menu, reenable timers and media poll thread
 *
 * Must be called after unmounting the image media, otherwise
 * there is a deadlock possibility scanMutex vs. mountMutex.
 */
void MainWindow::reenableImageChoice()
{
    _networkStatusPollTimer.start();
    _mediaPollThread->scanMutex.unlock();
    setEnabled(true);
}

void MainWindow::startImageWrite(QVariantMap &entry)
{
    /* All meta files downloaded, extract slides tarball, and launch image writer thread */
    enum ImageSource imageSource = entry.value("source").value<enum ImageSource>();
    QString folder = entry.value("folder").toString();
    QString slidesFolder = "/var/volatile/marketing/";
    QStringList slidesFolders;

    if (entry.contains("license") && !(_isAutoinstall || _acceptAllLicenses)) {
        QByteArray text = getFileContents(folder + "/" + entry.value("license").toString());
        ScrollTextDialog eula(entry.value("license_title").toString(), QString(text), QDialogButtonBox::Yes | QDialogButtonBox::Abort);
        eula.setButtonText(QDialogButtonBox::Yes, tr("I Agree"));
        eula.setDefaultButton(QDialogButtonBox::Abort);
        int ret = eula.exec();

        if (ret != QDialogButtonBox::Yes) {
            clearInstallSettings();
            reenableImageChoice();
            _TeziState = TEZI_ABORTED;
            return;
        }
    }

    if (entry.contains("releasenotes") && !(_isAutoinstall || _acceptAllLicenses)) {
        QByteArray text = getFileContents(folder + "/" + entry.value("releasenotes").toString());
        ScrollTextDialog releasenotes(entry.value("releasenotes_title").toString(), QString(text), QDialogButtonBox::Ok | QDialogButtonBox::Abort);
        releasenotes.setDefaultButton(QDialogButtonBox::Abort);
        int ret = releasenotes.exec();

        if (ret != QDialogButtonBox::Ok) {
            clearInstallSettings();
            reenableImageChoice();
            _TeziState = TEZI_ABORTED;
            return;
        }
    }

    /* Delete old slides if exist */
    if (QFile::exists(slidesFolder))
        removeDir(slidesFolder);

    QDir().mkdir(slidesFolder);

    if (entry.contains("marketing"))
    {
        QString marketingTar = folder + "/marketing.tar";
        if (QFile::exists(marketingTar))
        {
            /* Extract tarball with slides */
            QProcess::execute("tar xf " + marketingTar + " -C " + slidesFolder);
            if (!_installingFromMedia)
                QFile::remove(marketingTar);
        }
    }

    slidesFolders.clear();
    if (QFile::exists(slidesFolder + "/slides_vga"))
        slidesFolders.append(slidesFolder + "/slides_vga");

    _imageWriteThread = new MultiImageWriteThread(_toradexConfigBlock, _moduleInformation);
    _imageWriteThread->setImage(folder, entry.value("image_info").toString(),
                               entry.value("baseurl").toString(), imageSource);

    _psd = new ProgressSlideshowDialog(slidesFolders, "", 20, this);
    connect(_imageWriteThread, SIGNAL(parsedImagesize(qint64)), _psd, SLOT(setMaximum(qint64)));
    connect(_imageWriteThread, SIGNAL(imageProgress(qint64)), _psd, SLOT(updateIOstats(qint64)));
    connect(_imageWriteThread, SIGNAL(completed()), this, SLOT(onCompleted()));
    connect(_imageWriteThread, SIGNAL(error(QString)), this, SLOT(onError(QString)));
    connect(_imageWriteThread, SIGNAL(statusUpdate(QString)), _psd, SLOT(setLabelText(QString)));
    _imageWriteThread->start();
    if (_ld != nullptr)
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

void MainWindow::on_list_itemSelectionChanged()
{
    // Hack to recalculate item sizes
    auto iconSize = ui->list->iconSize();
    QListWidgetItem *item;

    for (int i = 0; i != ui->list->count(); ++i) {
        item = ui->list->item(i);
        if (item->isSelected()) {
              item->setSizeHint(iconSize + QSize(1,1));
        } else {
              item->setSizeHint(iconSize);
        }
    }
}
