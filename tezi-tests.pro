QT += testlib
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

GIT_VERSION = $$system(git --git-dir $$PWD/.git --work-tree $$PWD rev-parse --short HEAD)
DEFINES += GIT_VERSION=\\\"$$GIT_VERSION\\\"

SOURCES +=  tests/tst_testconfigblock.cpp \
    configblock.cpp \
    util.cpp

HEADERS += \
    config.h \
    configblock.h \
    util.h
