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
#include <QMainWindow>
#include <QModelIndex>
#include <QSplashScreen>
#include <QMessageBox>
#include <QTimer>
#include <QTime>
#include <QUrl>
#include <QSet>
#include <QFileSystemWatcher>

enum RebootMode {
    LINUX_UNKNOWN,
    LINUX_REBOOT,
    LINUX_POWEROFF
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

public:
    explicit MainWindow(QSplashScreen *splash, LanguageDialog* ld, bool allowAutoinstall, QWidget *parent = 0);
    ~MainWindow();
    void show();
    void showProgressDialog(const QString &labelText);
    bool initialize();

protected:
    Ui::MainWindow *ui;
    QFileSystemWatcher *_fileSystemWatcher;
    QProgressDialog *_qpd;
    ProgressSlideshowDialog *_psd;
    ModuleInformation *_moduleInformation;
    ConfigBlock *_toradexConfigBlock;
    QString _toradexProductName, _toradexBoardRev, _serialNumber, _toradexProductNumber;
    int _toradexProductId;
    bool _allowAutoinstall, _isAutoinstall, _showAll, _newInstallerAvailable;
    QSplashScreen *_splash;
    LanguageDialog *_ld;
    bool _wasOnline, _wasRndis;
    bool _downloadNetwork, _downloadRndis;
    int _imageListDownloadsActive;
    QNetworkAccessManager *_netaccess;
    int _neededMB, _availableMB, _numMetaFilesToDownload;
    bool _installingFromMedia;
    QTimer _networkStatusPollTimer;
    QTime _time;
    QIcon _sdIcon,_usbIcon, _internetIcon;
    QVariantMap _imageEntry;
    UsbGadget *_usbGadget;
    MediaPollThread *_mediaPollThread;
    MultiImageWriteThread *_imageWriteThread;
    QList<FeedServer> _networkFeedServerList;

    void setWorkingInBackground(bool working, const QString &labelText = "");
    void updateModuleInformation();
    void updateVersion();
    virtual void changeEvent(QEvent * event);
    void removeTemporaryFiles(const QVariantMap entry);
    void removeImagesBySource(enum ImageSource source);
    bool hasAddress(const QString &iface, QNetworkAddressEntry *currAddress = NULL);
    QStringList getFlavours(const QString &folder);
    QListWidgetItem *findItem(const QVariant &name);
    QList<QListWidgetItem *> selectedItems();
    void updateNeeded();
    void downloadMetaFile(const QString &url, const QString &saveAs);
    bool downloadLists(const enum ImageSource source);
    void installImage(QVariantMap entry);
    void startImageWrite(QVariantMap &entry);
    bool validateImageJson(QVariantMap &entry);
    void reenableImageChoice();

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
    void removeImagesByBlockdev(const QString blockdev);
    void addNewImageUrl(const QString url);
    void addImages(const QListVariantMap images);
    void errorMounting(const QString blockdev);

    /* Events from ImageListDownload */
    void onImageListDownloadFinished();
    void onImageListDownloadError(const QString &msg);

    /* Events from ImageWriterThread */
    void onError(const QString &msg);
    void onCompleted();
    void onQuery(const QString &msg, const QString &title, QMessageBox::StandardButton* answer);

    /* QFileSystemWatcher */
    void inputDirectoryChanged(const QString& path);

private slots:
    /* UI events */
    void on_actionInstall_triggered();
    void on_actionRefreshCloud_triggered();
    void on_actionCancel_triggered();
    void on_actionAdvanced_triggered(bool checked);
    void on_actionUsbMassStorage_triggered(bool checked);
    void on_actionUsbRndis_triggered(bool checked);
    void on_actionEraseModule_triggered();
    void on_actionShowLicense_triggered();
    void on_actionEditFeeds_triggered();
    void on_actionEdit_config_triggered();
    void on_list_currentItemChanged();
    void on_list_itemDoubleClicked();
    void on_actionWifi_triggered();
};

#endif // MAINWINDOW_H
