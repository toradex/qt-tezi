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
#include "multiimagewritethread.h"
#include "moduleinformation.h"
#include <QMainWindow>
#include <QModelIndex>
#include <QSplashScreen>
#include <QMessageBox>
#include <QTimer>
#include <QTime>
#include <QUrl>
#include <QSet>

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

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QSplashScreen *splash, LanguageDialog* ld, bool allowAutoinstall, QWidget *parent = 0);
    ~MainWindow();
    void show();
    void showProgressDialog(const QString &labelText);
    void startNetworking();

protected:
    Ui::MainWindow *ui;
    QProgressDialog *_qpd;
    ProgressSlideshowDialog *_psd;
    ModuleInformation *_moduleInformation;
    ConfigBlock *_toradexConfigBlock;
    QByteArray *_nandBootBlock;
    QString _targetDevice, _targetDeviceClass, _targetDeviceCfgBlock;
    QString _toradexProductName, _toradexBoardRev, _serialNumber, _toradexProductNumber;
    int _toradexProductId;
    bool _allowAutoinstall, _isAutoinstall, _showAll, _newInstallerAvailable;
    QSplashScreen *_splash;
    LanguageDialog *_ld;
    bool _wasOnline, _wasRndis;
    QNetworkAccessManager *_netaccess;
    int _neededMB, _availableMB, _numDownloads, _numMetaFilesToDownload;
    QTimer _networkStatusPollTimer;
    QTimer _mediaPollTimer;
    QTime _time;
    QIcon _sdIcon,_usbIcon, _internetIcon;
    QVariantMap _imageEntry;
    bool _mediaMounted, _firstMediaPoll;
    QSet<QString> _blockdevsChecked;
    QSet<QString> _blockdevsChecking;
    UsbGadget *_usbGadget;
    MultiImageWriteThread *_imageWriteThread;
    QList<QVariantMap> _netImages;
    QStringList _networkUrlList;
    QStringList _rndisUrlList;

    void updateModuleInformation();
    void updateVersion();
    int calculateNominalSize(const QVariantMap &imagemap);
    void processMedia(enum ImageSource src, const QString &blockdev);
    void parseTeziConfig(const QString &path);
    QList<QVariantMap> listMediaImages(const QString &path, const QString &blockdev, enum ImageSource source);
    virtual void changeEvent(QEvent * event);
    bool isMounted(const QString &path);
    bool mountMedia(const QString &blockdev);
    bool unmountMedia();
    void checkRemovableBlockdev(const QString &nameFilter);
    void checkSDcard();
    static bool orderByIndex(const QVariantMap &m1, const QVariantMap &m2);
    void addImages(QList<QVariantMap> images);
    void removeTemporaryFiles(const QVariantMap entry);
    void removeImagesByBlockdev(const QString &blockdev);
    void removeImagesBySource(enum ImageSource source);
    bool hasAddress(const QString &iface, QNetworkAddressEntry *currAddress = NULL);
    QStringList getFlavours(const QString &folder);
    QListWidgetItem *findItem(const QVariant &name);
    QList<QListWidgetItem *> selectedItems();
    void updateNeeded();
    void downloadMetaFile(const QString &url, const QString &saveAs);
    void downloadLists(const QStringList &urls);
    void installImage(QVariantMap entry);
    void startImageWrite(QVariantMap entry);
    void reenableImageChoice();

signals:
    void abortAllDownloads();

protected slots:
    void pollMedia();
    void pollNetworkStatus();
    void downloadListJsonCompleted();
    void downloadListJsonFailed();
    void downloadImageJsonCompleted();
    void downloadImageJsonFailed();
    void downloadIconCompleted();
    void downloadIconFailed();
    void downloadMetaCompleted();
    void downloadMetaFailed();
    void downloadFinished();
    void eraseMtd();
    void eraseFinished();
    void discardBlockdev();
    void discardFinished();
    void discardOrEraseError(const QString &errorString);

    /* Events from ImageWriterThread */
    void onError(const QString &msg);
    void onCompleted();
    void onQuery(const QString &msg, const QString &title, QMessageBox::StandardButton* answer);

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
    void on_actionEdit_config_triggered();
    void on_list_currentItemChanged();
    void on_list_itemDoubleClicked();
    void on_actionWifi_triggered();
};

#endif // MAINWINDOW_H
