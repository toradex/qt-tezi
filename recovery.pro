#-------------------------------------------------
#
# Project created by QtCreator 2013-04-30T12:10:31
#
#-------------------------------------------------

QT       += core gui network dbus

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = recovery
target.files = recovery
target.path = /var/volatile
INSTALLS = target
TEMPLATE = app
LIBS += -lqjson

system(sh updateqm.sh 2>/dev/null)

SOURCES += main.cpp\
        mainwindow.cpp \
    languagedialog.cpp \
    initdrivethread.cpp \
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
    filesysteminfo.cpp

HEADERS  += mainwindow.h \
    languagedialog.h \
    initdrivethread.h \
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
    filesysteminfo.h

FORMS    += mainwindow.ui

RESOURCES += \
    icons.qrc

TRANSLATIONS += translation_nl.ts \
    translation_de.ts \
    translation_pt.ts \
    translation_ja.ts \
    translation_fr.ts \
    translation_hu.ts \
    translation_fi.ts

OTHER_FILES += \
    README.txt \
    wpa_supplicant/wpa_supplicant.xml
