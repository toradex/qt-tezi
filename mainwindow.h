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
    explicit MainWindow(QSplashScreen *splash, LanguageDialog* ld, QString &toradexProductId, QString &toradexBoardRev,
                        bool allowAutoinstall, QWidget *parent = 0);
    ~MainWindow();
    void showProgressDialog();
    void startNetworking();

protected:
    Ui::MainWindow *ui;
    QProgressDialog *_qpd;
    ProgressSlideshowDialog *_psd;
    QString _model, _toradexProductId, _toradexBoardRev;
    bool _allowAutoinstall, _isAutoinstall, _showAll;
    QSplashScreen *_splash;
    LanguageDialog *_ld;
    QSettings *_settings;
    bool _hasWifi;
    QNetworkAccessManager *_netaccess;
    int _neededMB, _availableMB, _numMetaFilesToDownload, _numIconsToDownload;
    QTimer _networkStatusPollTimer;
    QTimer _mediaPollTimer;
    QTime _time;
    QIcon _sdIcon,_usbIcon, _internetIcon;
    QVariantMap _imageEntry;
    bool _mediaMounted;
    QSet<QString> _blockdevsChecked;
    QSet<QString> _blockdevsChecking;

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
    void update_window_title();
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

    /* Events from ImageWriterThread */
    void onError(const QString &msg);
    void onCompleted();
    void onQuery(const QString &msg, const QString &title, QMessageBox::StandardButton* answer);
    void hideDialogIfNoNetwork();

private slots:
    /* UI events */
    void on_actionInstall_triggered();
    void on_actionCancel_triggered();
    void on_actionAdvanced_triggered(bool checked);
    void on_actionEdit_config_triggered();
    void on_actionBrowser_triggered();
    void on_list_currentItemChanged();
    void on_list_itemDoubleClicked();
    void on_actionWifi_triggered();

signals:
    void networkUp();
};

#endif // MAINWINDOW_H
