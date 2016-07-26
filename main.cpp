#include "mainwindow.h"
#include "languagedialog.h"
#include "config.h"
#include "keydetection.h"
#include "gpioinput.h"
#include "rightbuttonfilter.h"
#include "longpresshandler.h"
#include "json.h"
#include "util.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <QApplication>
#include <QStyle>
#include <QDesktopWidget>
#include <QSplashScreen>
#include <QFile>
#include <QIcon>
#include <QProcess>
#include <QDir>
#include <QDebug>
#include <QTime>
#include <QMessageBox>

#ifdef Q_WS_QWS
#include <QWSServer>
#endif

/*
 * Author: Stefan Agner
 * Initial author: Floris Bos
 *
 * See LICENSE.txt for license details
 *
 */

void reboot()
{
#ifdef Q_WS_QWS
    QWSServer::setBackground(BACKGROUND_COLOR);
    QWSServer::setCursorVisible(true);
#endif

    // Shut down networking
    QProcess::execute("ifdown -a");
    // Unmount file systems
    QProcess::execute("umount -ar");
    ::sync();
    // Reboot
    ::reboot(RB_AUTOBOOT);
}

int main(int argc, char *argv[])
{
    bool hasTouchScreen = QFile::exists("/sys/devices/platform/rpi_ft5406");

    // Unless we have a touch screen, wait for keyboard to appear before displaying anything
    if (!hasTouchScreen)
        KeyDetection::waitForKeyboard();

    QApplication a(argc, argv);
    RightButtonFilter rbf;
    LongPressHandler lph;

    bool autoinstall = false;

    QString defaultLang = "en";
    QString defaultKeyboard = "us";

    // Process command-line arguments
    for (int i=1; i<argc; i++)
    {
        // Flag to indicate first boot
        if (strcmp(argv[i], "-autoinstall") == 0)
            autoinstall = true;
    }

    // Intercept right mouse clicks sent to the title bar
    a.installEventFilter(&rbf);

    // Treat long holds as double-clicks
    if (hasTouchScreen)
        a.installEventFilter(&lph);

#ifdef Q_WS_QWS
    QWSServer::setCursorVisible(false);
#endif

    QDir settingsdir;
    settingsdir.mkdir("/settings");

    // Set wallpaper and icon, if we have resource files for that
    if (QFile::exists(":/icons/toradex_icon.png"))
        a.setWindowIcon(QIcon(":/icons/toradex_icon.png"));

#ifdef Q_WS_QWS
        QWSServer::setBackground(BACKGROUND_COLOR);
#endif
        QSplashScreen *splash = new QSplashScreen(QPixmap(":/wallpaper.png"));
        splash->show();
        QApplication::processEvents();

    // If -runinstaller is not specified, only continue if SHIFT is pressed, GPIO is triggered,
    // or no OS is installed (/settings/installed_os.json does not exist)
        /*
    bool bailout = !runinstaller
        && !force_trigger;

    if (bailout && keyboard_trigger)
    {
        QTime t;
        t.start();

        while (t.elapsed() < 2000)
        {
            QApplication::processEvents(QEventLoop::WaitForMoreEvents, 10);
            if (QApplication::queryKeyboardModifiers().testFlag(Qt::ShiftModifier))
            {
                bailout = false;
                qDebug() << "Shift detected";
                break;
            }
            if (hasTouchScreen && QApplication::mouseButtons().testFlag(Qt::LeftButton))
            {
                bailout = false;
                qDebug() << "Tap detected";
                break;
            }
        }
    }
    bailout = false;*/

#ifdef Q_WS_QWS
    QWSServer::setCursorVisible(true);
#endif

    QString toradexProductId = getFileContents("/proc/device-tree/toradex,product-id");
    QString toradexBoardRev = getFileContents("/proc/device-tree/toradex,board-rev");

    qDebug() << "Product id is " << toradexProductId;
    qDebug() << "Board revision is " << toradexBoardRev;

    if (toradexProductId.isEmpty()) {
        QMessageBox::critical(NULL, QObject::tr("Reading Product ID failed"),
                              QObject::tr("Reading the Toradex Product ID failed, the Toradex config block might be corrupted."),
                              QMessageBox::Close);
        return 1;
    }

    // Main window in the middle of screen
    MainWindow mw(splash, toradexProductId, toradexBoardRev, autoinstall);
    mw.setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, mw.size(), a.desktop()->availableGeometry()));
    mw.show();
    mw.showProgressDialog();
    mw.startNetworking();

#ifdef ENABLE_LANGUAGE_CHOOSER
     // Language chooser at the bottom center
    LanguageDialog* ld = new LanguageDialog(defaultLang, defaultKeyboard);
    ld->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignHCenter | Qt::AlignBottom, ld->size(), a.desktop()->availableGeometry()));
    ld->show();
#endif

    a.exec();

    reboot();

    return 0;
}
