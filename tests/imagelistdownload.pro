QT += core network testlib
QT -= gui
CONFIG += console testcase c++11
CONFIG -= app_bundle
TEMPLATE = app
TARGET = tst_imagelistdownload
INCLUDEPATH += ..
DEFINES += GIT_VERSION=\\\"test\\\"

SOURCES += tst_imagelistdownload.cpp \
    feed_test_support.cpp \
    ../imagelistdownload.cpp \
    ../resourcedownload.cpp \
    ../imagelist.cpp \
    ../configblock.cpp \
    ../util.cpp
HEADERS += ../imagelistdownload.h \
    ../resourcedownload.h \
    ../imagelist.h \
    ../configblock.h
