#ifndef HTTPAPI_H
#define HTTPAPI_H

#include <qhttpserver.hpp>
#include <qhttpserverrequest.hpp>
#include <qhttpserverresponse.hpp>
#include <qhttpfwd.hpp>
#include <QNetworkAccessManager>
#include <QObject>
#include "imagesource.h"

using namespace qhttp::server;

class MainWindow;

class HttpApi : public QObject
{
    Q_OBJECT

public:
    HttpApi(QObject *parent = nullptr);
    void initialize(MainWindow *mainWindow);
    int start(QString port, MainWindow *mainWindow);

protected:
    qhttp::server::QHttpServer *_httpServer;

signals:
    void httpApiFoundAutoInstallImage(const QVariantMap &image);
    void httpApiDownloadImage(const QString url, enum ImageSource source);
    void newImageUrl(const QString url);
};

#endif // HTTPAPI_H
