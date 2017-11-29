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
    _splash(splash), _ld(ld), _wasOnline(false), _wasRndis(false), _downloadNetwork(true), _downloadRndis(true),
    _imageListDownloadsActive(0), _netaccess(NULL)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setWindowState(Qt::WindowMaximized);
    setContextMenuPolicy(Qt::NoContextMenu);
    ui->setupUi(this);
}

bool MainWindow::initialize() {
    _moduleInformation = ModuleInformation::detectModule(this);
    if (_moduleInformation == NULL) {
        QMessageBox::critical(NULL, QObject::tr("Module Detection failed"),
                              QObject::tr("Failed to detect the basic module type. Cannot continue."),
                              QMessageBox::Close);
        return false;
    }

    if (!_moduleInformation->moduleSupported()) {
        QMessageBox::critical(NULL, QObject::tr("Unsupported Module Type"),
                              QObject::tr("This module is an early sample which is not supported by the Toradex Easy Installer due to chip issues."),
                              QMessageBox::Close);
        return false;
    }
    _toradexConfigBlock = _moduleInformation->readConfigBlock();

    // Unlock by default
    _moduleInformation->unlockFlash();

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

    _mediaPollThread = new MediaPollThread(_moduleInformation, this);

    ui->list->setSelectionMode(QAbstractItemView::SingleSelection);

    _availableMB = _moduleInformation->getStorageSize() / (1024 * 1024);

    _usbGadget = new UsbGadget(_serialNumber, _toradexProductName, _toradexProductId);

    if (_moduleInformation->storageClass() == ModuleInformation::StorageClass::Block) {
        if (_usbGadget->initMassStorage(_moduleInformation->mainPartition()))
            ui->actionUsbMassStorage->setEnabled(true);
        else
            ui->actionUsbMassStorage->setEnabled(false);
    } else {
        ui->mainToolBar->removeAction(ui->actionUsbMassStorage);
    }

    if (_usbGadget->initRndis()) {
        ui->actionUsbRndis->setEnabled(true);
        ui->actionUsbRndis->trigger();

    } else {
        ui->actionUsbRndis->setEnabled(false);
    }

    // Add static server list
    FeedServer srv;
    srv.label = "Toradex Image Server";
    srv.url = DEFAULT_IMAGE_SERVER;
    srv.enabled = true;
    _networkFeedServerList.append(srv);

    srv.label = "Toradex 3rd Party Image Server";
    srv.url = DEFAULT_3RDPARTY_IMAGE_SERVER;
    srv.enabled = true;
    _networkFeedServerList.append(srv);

    srv.label = "Toradex Continous Integration Server (testing)";
    srv.url = DEFAULT_CI_IMAGE_SERVER;
    srv.enabled = false;
    _networkFeedServerList.append(srv);

    qRegisterMetaType<QListVariantMap>("QListVariantMap");

    // Starting network after first media scan makes sure that a local media takes presedence over a server provided image.
    connect(_mediaPollThread, SIGNAL (firstScanFinished()), this, SLOT (startNetworking()));
    connect(_mediaPollThread, SIGNAL (removedBlockdev(const QString)), this, SLOT (removeImagesByBlockdev(const QString)));
    connect(_mediaPollThread, SIGNAL (newImageUrl(const QString)), this, SLOT (addNewImageUrl(const QString)));
    connect(_mediaPollThread, SIGNAL (newImagesToAdd(const QListVariantMap)), this, SLOT (addImages(const QListVariantMap)));
    connect(_mediaPollThread, SIGNAL (errorMounting(const QString)), this, SLOT (errorMounting(const QString)));

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
        ui->waitingSpinner->start();
        ui->labelBackgroundTask->setText(labelText);
    } else {
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
    setWorkingInBackground(true, tr("Wait for external media or network..."));
}

void MainWindow::showProgressDialog(const QString &labelText)
{
    /* Ask user to wait while list is populated */
    _qpd = new QProgressDialog(labelText, QString(), 0, 0, this);
    _qpd->setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    _qpd->setModal(true);
    _qpd->show();
}

void MainWindow::addImages(const QListVariantMap images)
{
    ImageListDownload *ild = qobject_cast<ImageListDownload *>(sender());
    QSize currentsize = ui->list->iconSize();
    bool isAutoinstall = false;
    int feedindex = -1; // Use -1 so that local media will be displayed first.
    QVariantMap autoInstallImage;

    if (ild != NULL)
        feedindex = ild->index();

    foreach (const QVariantMap m, images)
    {
        int config_format = m.value("config_format").toInt();
        int imageindex = m.value("index").toInt();
        QString name = m.value("name").toString();
        QString foldername = m.value("foldername").toString();
        QString description = m.value("description").toString();
        QString version = m.value("version").toString();
        QString releasedate = m.value("release_date").toString();
        QByteArray icondata = m.value("iconimage").value<QByteArray>();
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

        QString friendlyname = name;
        if (!supportedImage)
            friendlyname += " [" + tr("not compatible with this module") + "]";
        else if (!supportedConfigFormat)
            friendlyname += " [" + tr("requires a newer version of the installer") + "]";

        friendlyname += "\n";
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

        QPixmap pix;
        pix.loadFromData(icondata);
        QIcon icon(pix);
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

        QListImageWidgetItem *item = new QListImageWidgetItem(icon, friendlyname, feedindex, imageindex);

        item->setData(Qt::UserRole, m);
        item->setToolTip(description);

        if (supportedImage && supportedConfigFormat)
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        else
            item->setFlags(Qt::NoItemFlags);

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
    ui->list->sortItems();

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

    /* Remove waiting spinner as soon as there is something the user might want to look at... */
    if (ui->list->count() > 0) {
        if (_imageListDownloadsActive == 0)
            setWorkingInBackground(false);
    } else {
        setWorkingInBackground(true, tr("Wait for external media or network..."));
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

void MainWindow::removeImagesByBlockdev(const QString blockdev)
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

void MainWindow::addNewImageUrl(const QString url)
{
    FeedServer server;
    server.label = tr("Custom Server from Media");
    server.url = url;
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

    if (url.contains(RNDIS_ADDRESS)) {
        _downloadRndis = true;
        removeImagesBySource(SOURCE_RNDIS);
    } else {
        _downloadNetwork = true;
        removeImagesBySource(SOURCE_NETWORK);
    }
}


void MainWindow::on_list_currentItemChanged()
{
    updateNeeded();
}

void MainWindow::on_list_itemDoubleClicked()
{
    on_actionInstall_triggered();
}

void MainWindow::errorMounting(const QString blockdev)
{
    QMessageBox::critical(this, tr("Error mounting"),
                          tr("Error mounting external device (%1)").arg(blockdev),
                          QMessageBox::Close);
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

void MainWindow::installImage(QVariantMap entry)
{
    enum ImageSource imageSource = (enum ImageSource)entry.value("source").value<int>();

    setEnabled(false);
    _numMetaFilesToDownload = 0;

    /* Re-mount local media */
    _installingFromMedia = !ImageInfo::isNetwork(imageSource);
    _mediaPollThread->scanMutex.lock();
    if (_installingFromMedia) {
        QString blockdev = entry.value("image_source_blockdev").value<QString>();
        if (!_mediaPollThread->mountMedia(blockdev)) {
            errorMounting(blockdev);
            _mediaPollThread->scanMutex.unlock();
            setEnabled(true);
            return;
        }
    }

    if (entry.value("validate", true).value<bool>()) {
        if (!validateImageJson(entry)) {
            if (_installingFromMedia)
                _mediaPollThread->unmountMedia();

            reenableImageChoice();
            return;
        }
    }

    /* Stop network polling, we are about to install a image (media polling is protected due to "mount singleton") */
    _networkStatusPollTimer.stop();
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

    /* If asynchrounous meta data download is in progress, wait for them to finish... */
    if (_numMetaFilesToDownload == 0)
        startImageWrite(entry);
    else
        showProgressDialog(tr("The install process will begin shortly."));
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
                               "After this operation you either need to install an image or use the modules recovery mode to boot back into Toradex Easy Installer. Continue?"),
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
                               "After this operation you either need to install an image or use the modules recovery mode to boot back into Toradex Easy Installer. Continue?"),
                            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes)
    {
        showProgressDialog(tr("Discarding all data on internal eMCC..."));
        /*
         * Note: In the eMMC case we discard all partitons. It is not easy to carve out
         * small portitions of blocks since discard seems to operate on erase bock size.
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
        installImage(entry);
    }
}

void MainWindow::on_actionRefreshCloud_triggered()
{
    removeImagesBySource(SOURCE_NETWORK);
    if (hasAddress("eth0")) {
        downloadLists(SOURCE_NETWORK);
    }

    removeImagesBySource(SOURCE_RNDIS);
    if (hasAddress("usb0")) {
        downloadLists(SOURCE_RNDIS);
    }
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
    _psd->close();
    _psd->deleteLater();
    _psd = NULL;
    _imageWriteThread->deleteLater();


    if (_installingFromMedia)
        _mediaPollThread->unmountMedia();

    /* Directly reboot into newer Toradex Easy Installer */
    if (_imageWriteThread->getImageInfo()->isInstaller() && _isAutoinstall) {
        close();
        /* A case for kexec... Anyone? :-) */
        QApplication::exit(LINUX_REBOOT);
    }

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
        if (_ld != NULL)
            _ld->show();
        break;
    }
}

void MainWindow::onError(const QString &msg)
{
    qDebug() << "Error:" << msg;

    if (_installingFromMedia)
        _mediaPollThread->unmountMedia();

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

            setWorkingInBackground(true, tr("Downloading image list ..."));

            _wasOnline = true;
        }

        if (_downloadNetwork) {
            _downloadNetwork = false;
            downloadLists(SOURCE_NETWORK);
        }
    } else {
        if (_wasOnline) {
            ui->labelEthernetAddress->setText(tr("No address assigned"));
            removeImagesBySource(SOURCE_NETWORK);
            _wasOnline = false;
            _downloadNetwork = true;
        }
    }

    if (hasAddress("usb0", &ae)) {

        if (!_wasRndis) {
            qDebug() << "RNDIS up in" << _time.elapsed()/1000.0 << "seconds";
            ui->labelRNDISAddress->setText(
                        QString("%2/%3").arg(ae.ip().toString(), QString::number(ae.prefixLength())));

            _wasRndis = true;
        }

        if (_downloadRndis) {
            _downloadRndis = false;
            downloadLists(SOURCE_RNDIS);
        }

    } else {
        if (_wasRndis) {
            ui->labelRNDISAddress->setText(tr("No address assigned"));
            removeImagesBySource(SOURCE_RNDIS);
            _wasRndis = false;
            _downloadRndis = true;
        }
    }
}

bool MainWindow::downloadLists(const enum ImageSource source)
{
    bool downloading = false;
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

    foreach (FeedServer server, _networkFeedServerList)
    {
        bool isRndis = server.url.contains(RNDIS_ADDRESS);

        switch (source) {
        case SOURCE_NETWORK:
            if (isRndis)
                continue;
            break;
        case SOURCE_RNDIS:
            if (!isRndis)
                continue;
        default:
            continue;
        }

        if (!server.enabled)
            continue;

        ImageListDownload *imageList = new ImageListDownload(server.url, index, _netaccess, this);
        connect(imageList, SIGNAL(newImagesToAdd(const QListVariantMap)), this, SLOT(addImages(const QListVariantMap)));
        connect(imageList, SIGNAL(finished()), this, SLOT(onImageListDownloadFinished()));
        connect(imageList, SIGNAL(error(QString)), this, SLOT(onImageListDownloadError(QString)));
        _imageListDownloadsActive++;
        index++;
    }

    if (_imageListDownloadsActive > 0)
        setWorkingInBackground(true, tr("Reloading images from network..."));

    return _imageListDownloadsActive > 0;
}

void MainWindow::onImageListDownloadFinished()
{
    if (_imageListDownloadsActive)
        _imageListDownloadsActive--;

    if (_imageListDownloadsActive == 0)
        setWorkingInBackground(false);
}

void MainWindow::onImageListDownloadError(const QString &msg)
{
    qDebug() << "Error:" << msg;

    QMessageBox::critical(this, tr("Error"), msg, QMessageBox::Ok);
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
    _imageWriteThread = new MultiImageWriteThread(_toradexConfigBlock, _moduleInformation);
    enum ImageSource imageSource = (enum ImageSource)entry.value("source").toInt();
    QString folder = entry.value("folder").toString();
    QStringList slidesFolders;

    if (entry.contains("license") && !_isAutoinstall) {
        QByteArray text = getFileContents(folder + "/" + entry.value("license").toString());
        ScrollTextDialog eula(entry.value("license_title").toString(), QString(text), QDialogButtonBox::Yes | QDialogButtonBox::Abort);
        eula.setButtonText(QDialogButtonBox::Yes, tr("I Agree"));
        eula.setDefaultButton(QDialogButtonBox::Abort);
        int ret = eula.exec();

        if (ret != QDialogButtonBox::Yes) {
            if (_installingFromMedia)
                _mediaPollThread->unmountMedia();
            reenableImageChoice();
            return;
        }
    }

    if (entry.contains("releasenotes") && !_isAutoinstall) {
        QByteArray text = getFileContents(folder + "/" + entry.value("releasenotes").toString());
        ScrollTextDialog eula(entry.value("releasenotes_title").toString(), QString(text), QDialogButtonBox::Ok | QDialogButtonBox::Abort);
        eula.setDefaultButton(QDialogButtonBox::Abort);
        int ret = eula.exec();

        if (ret != QDialogButtonBox::Ok) {
            if (_installingFromMedia)
                _mediaPollThread->unmountMedia();
            reenableImageChoice();
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
