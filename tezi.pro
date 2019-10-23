# Qt Project for Toradex Installer

QT += core gui network dbus concurrent widgets

TARGET = tezi
target.files = tezi
target.path = /var/volatile
INSTALLS = target
TEMPLATE = app
CONFIG += link_pkgconfig
PKGCONFIG += libusbgx
LIBS += -lqtzeroconf-browser -lqtzeroconf-common

system(sh updateqm.sh 2>/dev/null)

GIT_VERSION = $$system(git --git-dir $$PWD/.git --work-tree $$PWD rev-parse --short HEAD)
DEFINES += GIT_VERSION=\\\"$$GIT_VERSION\\\"

SOURCES += main.cpp\
        mainwindow.cpp \
    languagedialog.cpp \
    gpioinput.cpp \
    progressslideshowdialog.cpp \
    confeditdialog.cpp \
    rightbuttonfilter.cpp \
    json.cpp \
    multiimagewritethread.cpp \
    util.cpp \
    twoiconsdelegate.cpp \
    longpresshandler.cpp \
    resourcedownload.cpp \
    scrolltextdialog.cpp \
    usbgadgethelper.c \
    usbgadget.cpp \
    configblock.cpp \
    discardthread.cpp \
    dto/imageinfo.cpp \
    dto/blockdevinfo.cpp \
    dto/rawfileinfo.cpp \
    dto/mtddevinfo.cpp \
    mtdnamedevicetranslator.cpp \
    dto/ubivolumeinfo.cpp \
    dto/contentinfo.cpp \
    dto/blockdevpartitioninfo.cpp \
    dto/winceimage.cpp \
    mtderasethread.cpp \
    configblockdialog.cpp \
    moduleinformation.cpp \
    mediapollthread.cpp \
    imagelistdownload.cpp \
    feedsdialog.cpp \
    waitingspinnerwidget.cpp \
    qlistimagewidgetitem.cpp

HEADERS  += mainwindow.h \
    languagedialog.h \
    config.h \
    gpioinput.h \
    progressslideshowdialog.h \
    confeditdialog.h \
    rightbuttonfilter.h \
    json.h \
    multiimagewritethread.h \
    util.h \
    twoiconsdelegate.h \
    longpresshandler.h \
    resourcedownload.h \
    scrolltextdialog.h \
    usbgadgethelper.h \
    usbgadget.h \
    configblock.h \
    discardthread.h \
    dto/imageinfo.h \
    dto/blockdevinfo.h \
    dto/rawfileinfo.h \
    dto/mtddevinfo.h \
    dto/mtddevcontentinfo.h \
    mtdnamedevicetranslator.h \
    dto/ubivolumeinfo.h \
    dto/contentinfo.h \
    dto/blockdevpartitioninfo.h \
    dto/winceimage.h \
    mtderasethread.h \
    configblockdialog.h \
    moduleinformation.h \
    mediapollthread.h \
    imagelistdownload.h \
    feedsdialog.h \
    feedserver.h \
    imagesource.h \
    waitingspinnerwidget.h \
    qlistimagewidgetitem.h

FORMS    += mainwindow.ui \
    languagedialog.ui \
    progressslideshowdialog.ui \
    confeditdialog.ui \
    configblockdialog.ui \
    feedsdialog.ui

# Avoid warnings from resources. Unfortunately applied to all files...
QMAKE_CXXFLAGS += -Wno-unused-variable
RESOURCES += \
    icons.qrc \
    translations.qrc

TRANSLATIONS += translation_de.ts \
    translation_pt.ts \
    translation_fr.ts \
    translation_it.ts

OTHER_FILES += \
    README.txt \
    wpa_supplicant/wpa_supplicant.xml \
    tezi.schema
