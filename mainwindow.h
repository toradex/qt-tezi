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
#include <QMainWindow>
#include <QModelIndex>
#include <QSplashScreen>
#include <QMessageBox>
#include <QTimer>
#include <QTime>

enum ImageSource {
    SOURCE_USB,
    SOURCE_SDCARD,
    SOURCE_NETWORK,
};

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
    explicit MainWindow(const QString &defaultDisplay, QSplashScreen *splash, QWidget *parent = 0);
    ~MainWindow();

protected:
    Ui::MainWindow *ui;
    QProgressDialog *_qpd;
    QList <int> _kc;
    int _kcpos;
    const QString _defaultDisplay;
    bool _silent, _allowSilent, _showAll;
    static bool _partInited;
    static int _currentMode;
    QSplashScreen *_splash;
    QSettings *_settings;
    bool _hasWifi;
    QNetworkAccessManager *_netaccess;
    int _neededMB, _availableMB, _numMetaFilesToDownload, _numIconsToDownload;
    QTimer _networkStatusPollTimer;
    QTimer _mediaPollTimer;
    QTime _time;
    QString _model;

    QMap<QString,QVariantMap> listMediaImages(const QString &path, enum ImageSource source);
    virtual void changeEvent(QEvent * event);
    void inputSequence();
    bool isMounted(const QString &path);
    bool mountMedia(const QString &media, const QString &dst);
    void addImages(QMap<QString,QVariantMap> images);
    void update_window_title();
    bool requireNetwork();
    bool isOnline();
    QStringList getFlavours(const QString &folder);
    QListWidgetItem *findItem(const QVariant &name);
    QList<QListWidgetItem *> selectedItems();
    void updateNeeded();
    void downloadMetaFile(const QString &url, const QString &saveAs);
    void downloadIcon(const QString &urlstring, const QString &originalurl);
    void downloadList(const QString &urlstring);
    void downloadLists();
    void startImageWrite();
    bool canInstallOs(const QString &name, const QVariantMap &values);
    bool isSupportedOs(const QString &name, const QVariantMap &values);

protected slots:
    void populate();
    void startBrowser();
    void startNetworking();
    void pollMedia();
    void pollNetworkStatus();
    void onOnlineStateChanged(bool online);
    void downloadListComplete();
    void processJson(QVariant json);
    void processJsonOs(const QString &name, QVariantMap &details, QSet<QString> &iconurls);
    /* Events from ImageWriterThread */
    void onError(const QString &msg);
    void onCompleted();
    void downloadIconComplete();
    void downloadMetaRedirectCheck();
    void downloadIconRedirectCheck();
    void downloadListRedirectCheck();
    void downloadMetaComplete();
    void onQuery(const QString &msg, const QString &title, QMessageBox::StandardButton* answer);
    void hideDialogIfNoNetwork();

private slots:
    /* UI events */
    void on_actionInstall_triggered();
    void on_actionCancel_triggered();
    void on_list_currentRowChanged();
    void on_actionAdvanced_triggered(bool checked);
    void on_actionEdit_config_triggered();
    void on_actionBrowser_triggered();
    void on_list_doubleClicked(const QModelIndex &index);
    void on_list_itemChanged(QListWidgetItem *item);
    void on_actionWifi_triggered();

signals:
    void networkUp();
};

#endif // MAINWINDOW_H
