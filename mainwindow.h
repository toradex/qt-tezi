#ifndef MAINWINDOW_H
#define MAINWINDOW_H

/* Main window
 *
 * Initial author: Floris Bos
 * Maintained by Raspberry Pi
 *
 * See LICENSE.txt for license details
 *
 */

#include "languagedialog.h"
#include "progressslideshowdialog.h"
#include "dto/imageinfo.h"
#include "usbgadget.h"
#include "configblock.h"
#include "mediapollthread.h"
#include "multiimagewritethread.h"
#include "moduleinformation.h"
#include "feedserver.h"
#include "imagelist.h"
#include "httpapi.h"
#include <QMainWindow>
#include <QModelIndex>
#include <QMessageBox>
#include <QTimer>
#include <QElapsedTimer>
#include <QUrl>
#include <QSet>
#include <QFileSystemWatcher>
#include <QHostInfo>
#include <qtzeroconf/zconfservicebrowser.h>
#include <atomic>

enum RebootMode {
    LINUX_UNKNOWN,
    LINUX_REBOOT,
    LINUX_POWEROFF
};

enum TeziState {
    TEZI_UNKNOWN,
    TEZI_IDLE,
    TEZI_SCANNING,
    TEZI_INSTALLING,
    TEZI_INSTALLED
};

namespace Ui {
class MainWindow;
}
class QProgressDialog;
class QSettings;
class QListWidgetItem;
class QNetworkAccessManager;
class QNetworkAddressEntry;
class QMessageBox;
class QFileInfo;

class MainWindow : public QMainWindow
{
    Q_OBJECT

private:
    QMutex  listMutex;
    QMutex  feedMutex;
public:
    explicit MainWindow(LanguageDialog* ld, bool allowAutoinstall, QWidget *parent = 0);
    ~MainWindow();
    void show();
    void showProgressDialog(const QString &labelText);
    bool initialize();
    ImageList *_imageList;
    QList<FeedServer> _networkFeedServerList;


    inline enum TeziState getTeziState() {
        return _TeziState;
    }

    QList<FeedServer> getFeedServerList() {
        return this->_networkFeedServerList;
    }

    QMutex* getFeedMutex() {
        return &this->feedMutex;
    }

protected:
    Ui::MainWindow *ui;
    QFileSystemWatcher *_fileSystemWatcher;
    QFileSystemWatcher *_fileSystemWatcherFb;
    QProgressDialog *_qpd;
    ProgressSlideshowDialog *_psd;
    ModuleInformation *_moduleInformation;
    ConfigBlock *_toradexConfigBlock;
    QString _toradexProductName, _toradexBoardRev, _serialNumber, _toradexProductNumber, _toradexPID8;
    int _toradexProductId;
    bool _allowAutoinstall, _isAutoinstall, _showAll;
    LanguageDialog *_ld;
    bool _wasOnNetwork, _wasRndis;
    bool _downloadNetwork, _downloadRndis;
    int _imageListDownloadsActive;
    QNetworkAccessManager *_netaccess;
    int _neededMB, _availableMB, _numMetaFilesToDownload;
    bool _installingFromMedia;
    QTimer _networkStatusPollTimer;
    QElapsedTimer _elapsedTimer;
    QIcon _sdIcon,_usbIcon, _networkIcon, _internetIcon;
    QVariantMap _imageEntry;
    UsbGadget *_usbGadget;
    MediaPollThread *_mediaPollThread;
    MultiImageWriteThread *_imageWriteThread;
    int _internetHostLookupId;
    ZConfServiceBrowser *_browser;
    HttpApi *_httpApi;
    std::atomic<TeziState> _TeziState;
    std::atomic<bool> _acceptAllLicenses;

    void setWorkingInBackground(bool working, const QString &labelText = "");
    void updateModuleInformation();
    void updateVersion();
    virtual void changeEvent(QEvent * event);
    void removeImagesBySource(enum ImageSource source);
    bool hasAddress(const QString &iface, QNetworkAddressEntry *currAddress = NULL);
    QStringList getFlavours(const QString &folder);
    QListWidgetItem *findItem(const QVariant &name);
    QList<QListWidgetItem *> selectedItems();
    void updateNeeded();
    void downloadMetaFile(const QString &url, const QString &saveAs);
    void downloadImageSetupSignals(ImageListDownload *imageListDownload);
    void downloadImageList(const QString &url, enum ImageSource source, int index);
    bool downloadLists(const enum ImageSource source);
    void installImage(QVariantMap entry);
    void startImageWrite(QVariantMap &entry);
    bool validateImageJson(QVariantMap &entry);
    void clearInstallSettings();
    void disableImageChoice();
    void reenableImageChoice();
    bool writeBootConfigurationBlock();
    bool setAvahiHostname(const QString hostname);

signals:
    void abortAllDownloads();

protected slots:
    void pollNetworkStatus();
    void downloadMetaCompleted();
    void downloadMetaFailed();
    void eraseMtd();
    void discardBlockdev();
    void discardOrEraseFinished();
    void discardOrEraseError(const QString &errorString);
    void startNetworking();
    void addNewImageUrl(const QString url);
    void imageListUpdated();
    void foundAutoInstallImage(const QVariantMap &image);
    void errorMounting(const QString blockdev);
    void disableFeed(const QString feedname);
    void imageHostLookupResults(QHostInfo hostInfo);

    /* Events from ImageListDownload */
    void onImageListDownloadFinished();
    void onImageListDownloadError(const QString &msg);

    /* Events from ImageWriterThread */
    void onError(const QString &msg);
    void onCompleted();
    void onQuery(const QString &msg, const QString &title, QMessageBox::StandardButton* answer);

    /* ZConfBrowser events */
    void addService(QString service);
    void removeService(QString service);

private slots:
    /* UI events */
    void on_actionInstall_triggered();
    void on_actionRefreshCloud_triggered();
    void on_actionCancel_triggered();
    void on_actionUsbMassStorage_triggered(bool checked);
    void on_actionUsbRndis_triggered(bool checked);
    void on_actionEraseModule_triggered();
    void on_actionShowLicense_triggered();
    void on_actionEditFeeds_triggered();
    void on_actionEdit_config_triggered();
    void on_list_currentItemChanged();
    void on_list_itemDoubleClicked();
    void on_actionWifi_triggered();
    void on_list_itemSelectionChanged();
    void downloadImage(const QVariantMap image);
    void downloadImage(const QString &url, enum ImageSource source);

    friend class HttpApi;
};

#endif // MAINWINDOW_H
