# Qt Project for Toradex Installer

QT       += core gui network dbus

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = tezi
target.files = tezi
target.path = /var/volatile
INSTALLS = target
TEMPLATE = app
LIBS += -lqjson

system(sh updateqm.sh 2>/dev/null)

SOURCES += main.cpp\
        mainwindow.cpp \
    languagedialog.cpp \
    keydetection.cpp \
    gpioinput.cpp \
    progressslideshowdialog.cpp \
    confeditdialog.cpp \
    rightbuttonfilter.cpp \
    json.cpp \
    multiimagewritethread.cpp \
    util.cpp \
    twoiconsdelegate.cpp \
    osinfo.cpp \
    partitioninfo.cpp \
    longpresshandler.cpp \
    filesysteminfo.cpp \
    blockdevinfo.cpp \
    resourcedownload.cpp

HEADERS  += mainwindow.h \
    languagedialog.h \
    config.h \
    keydetection.h \
    gpioinput.h \
    progressslideshowdialog.h \
    confeditdialog.h \
    rightbuttonfilter.h \
    json.h \
    multiimagewritethread.h \
    util.h \
    twoiconsdelegate.h \
    osinfo.h \
    partitioninfo.h \
    longpresshandler.h \
    filesysteminfo.h \
    blockdevinfo.h \
    resourcedownload.h

FORMS    += mainwindow.ui \
    languagedialog.ui \
    progressslideshowdialog.ui \
    confeditdialog.ui

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
    wpa_supplicant/wpa_supplicant.xml
