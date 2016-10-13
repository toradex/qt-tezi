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
#include "osinfo.h"
#include "usbgadget.h"
#include "configblock.h"
#include <QMainWindow>
#include <QModelIndex>
#include <QSplashScreen>
#include <QMessageBox>
#include <QTimer>
#include <QTime>
#include <QUrl>
#include <QSet>

namespace Ui {
class MainWindow;
}
class QProgressDialog;
class QSettings;
class QListWidgetItem;
class QNetworkAccessManager;
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
    ConfigBlock *_toradexConfigBlock;
    QString _toradexProductName, _toradexBoardRev, _serialNumber, _toradexProductNumber;
    int _toradexProductId;
    bool _allowAutoinstall, _isAutoinstall, _showAll;
    QSplashScreen *_splash;
    LanguageDialog *_ld;
    bool _wasOnline;
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

    void updateModuleInformation();
    void updateVersion();
    int calculateNominalSize(const QVariantMap &imagemap);
    void processMedia(enum ImageSource src, const QString &blockdev);
    QMap<QString,QVariantMap> listMediaImages(const QString &path, const QString &blockdev, enum ImageSource source);
    virtual void changeEvent(QEvent * event);
    void inputSequence();
    bool isMounted(const QString &path);
    bool mountMedia(const QString &blockdev);
    bool unmountMedia();
    void checkRemovableBlockdev(const QString &nameFilter);
    void checkSDcard();
    void addImages(QMap<QString,QVariantMap> images);
    void removeImagesByBlockdev(const QString &blockdev);
    void removeImagesBySource(enum ImageSource source);
    bool requireNetwork();
    bool isOnline();
    QStringList getFlavours(const QString &folder);
    QListWidgetItem *findItem(const QVariant &name);
    QList<QListWidgetItem *> selectedItems();
    void updateNeeded();
    void downloadMetaFile(const QString &url, const QString &saveAs);
    void downloadLists();
    void installImage(QVariantMap entry);
    void startImageWrite(QVariantMap entry);
    bool discard(QString blkdev, qint64 start, qint64 end);

protected slots:
    void startBrowser();
    void pollMedia();
    void pollNetworkStatus();
    void onOnlineStateChanged(bool online);
    void downloadListJsonCompleted();
    void downloadListJsonFailed();
    void downloadImageJsonCompleted();
    void downloadImageJsonFailed();
    void downloadIconCompleted();
    void downloadIconFailed();
    void downloadMetaCompleted();
    void downloadMetaFailed();
    void downloadFinished();
    void discardError(const QString &errorString);
    void discardFinished();

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
    void on_actionCleanModule_triggered();
    void on_actionShowLicense_triggered();
    void on_actionEdit_config_triggered();
    void on_actionBrowser_triggered();
    void on_list_currentItemChanged();
    void on_list_itemDoubleClicked();
    void on_actionWifi_triggered();

signals:
    void networkUp();
};

#endif // MAINWINDOW_H
