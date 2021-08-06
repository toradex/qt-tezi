#include "mainwindow.h"
#include "languagedialog.h"
#include "config.h"
#include "gpioinput.h"
#include "rightbuttonfilter.h"
#include "longpresshandler.h"
#include "json.h"
#include "util.h"
#include "configblock.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include <QApplication>
#include <QStyle>
#include <QDesktopWidget>
#include <QFile>
#include <QIcon>
#include <QProcess>
#include <QDir>
#include <QDebug>
#include <QElapsedTimer>
#include <QMessageBox>
#include <QFont>


/*
 * Author: Stefan Agner
 * Initial author: Floris Bos
 *
 * See LICENSE.txt for license details
 *
 */

void tezi_reboot(int mode)
{
    // Shut down networking
    QProcess::execute("ifdown -a");
    // Save system clock into file
    QProcess::execute("/etc/init.d/save-rtc.sh");
    // Unmount file systems
    QProcess::execute("umount -ar");
    ::sync();
    // Reboot
    ::reboot(mode);
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    RightButtonFilter rbf;
    LongPressHandler lph;

    bool autoinstall = false;
    bool fullscreen = false;
    bool hotplugfb = false;

    QString defaultLang = "en";
    QString defaultKeyboard = "us";

    // Process command-line arguments
    for (int i = 1; i < argc; i++)
    {
        // Flag to indicate first boot
        if (strcmp(argv[i], "-autoinstall") == 0)
            autoinstall = true;

        // Flag to indicate full screen
        if (strcmp(argv[i], "-fullscreen") == 0)
            fullscreen = true;
    }

    // Intercept right mouse clicks sent to the title bar
    a.installEventFilter(&rbf);

    // Treat long holds as double-clicks
    a.installEventFilter(&lph);

    // Set default font, required for Qt5/Wayland
    QFont font = QApplication::font();
    font.setPointSize(12);
    QApplication::setFont(font);


    // Set icon, if we have a resource file for that
    if (QFile::exists(":/icons/toradex_icon.png"))
        a.setWindowIcon(QIcon(":/icons/toradex_icon.png"));

    QApplication::processEvents();

    // If -runinstaller is not specified, only continue if SHIFT is pressed, GPIO is triggered,
    // or no OS is installed (/settings/installed_os.json does not exist)
        /*
    bool bailout = !runinstaller
        && !force_trigger;

    if (bailout && keyboard_trigger)
    {
        QElapsedTimer t;
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


    LanguageDialog* ld = NULL;
#ifdef ENABLE_LANGUAGE_CHOOSER
    // Language chooser at the bottom center
    ld = new LanguageDialog(defaultLang, defaultKeyboard);
    ld->setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignHCenter | Qt::AlignBottom, ld->size(), a.desktop()->availableGeometry()));
    ld->show();
#endif

    // Main window in the middle of screen
    MainWindow mw(ld, autoinstall);
    int mode = LINUX_POWEROFF;

    if (mw.initialize()) {
        if (fullscreen)
            mw.showFullScreen();
        else
            mw.show();
        mode = a.exec();
    }

    switch (mode) {
    case LINUX_REBOOT:
        tezi_reboot(RB_AUTOBOOT);
        break;
    case LINUX_POWEROFF:
    default:
        tezi_reboot(RB_POWER_OFF);
        break;
    }

    return 0;
}
