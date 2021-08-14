#include "httpapi.h"
#include "json.h"
#include "mainwindow.h"
#include "config.h"
#include "feedserver.h"

#include <QJsonDocument>
#include <QJsonObject>

using namespace qhttp;
using namespace qhttp::server;

HttpApi::HttpApi(QObject *parent) : QObject(parent),
    _httpServer(new qhttp::server::QHttpServer(parent))
{
}

/*
 * Warning: QHttp is running in a separate thread from the Qt UI, so calls to the
 * rest of the code need to be handled carefully.
 * The safest way to synchronize is probably through signals, signals should be
 * queued by default when coming from another thread (see `Qt::QueuedConnection`).
 * Otherwise make sure that data are properly protected by a mutex.
 */
int HttpApi::start(QString port, MainWindow *mainWindow)
{
    this->_httpServer->listen(port,
                              [mainWindow,this](qhttp::server::QHttpRequest *req, qhttp::server::QHttpResponse* res)
    {
        req->collectData();
        req->onEnd([req, res, mainWindow,this]() {
            TStatusCode status = qhttp::ESTATUS_OK;
            QByteArray returnValue;
            switch (req->method()) {
                case EHTTP_GET: {
                    if (req->url().path() == "/feeds") {
                        QVariantList list;
                        QMutexLocker locker(mainWindow->getFeedMutex());
                        for (FeedServer &fe : mainWindow->getFeedServerList()) {
                            QVariantMap feed;
                            feed["label"] = fe.label;
                            feed["url"] = fe.url;
                            feed["enabled"] = QString::number(fe.enabled);
                            list.append(feed);
                        }
                        returnValue = Json::serialize(QVariant(list));
                    } else if (req->url().path() == "/images") {
                        QVariantList list;
                        QMutexLocker locker(mainWindow->_imageList->GetMutex());
                        for (QVariantMap &m : mainWindow->_imageList->imageList()) {
                            QVariantMap image;
                            image["name"] = m["name"].value<QString>();
                            image["version"] = m["version"].value<QString>();
                            image["release_date"] = m["release_date"].value<QString>();
                            image["uri"] = m["uri"].value<QString>();
                            list.append(image);
                        }
                        returnValue = Json::serialize(QVariant(list));
                    } else if (req->url().path() == "/status"){
                        QVariantMap tezi_status;

                        switch (mainWindow->getTeziState()) {
                            case TEZI_IDLE:
                                tezi_status["status"] = "idle";
                                break;
                            case TEZI_SCANNING:
                                tezi_status["status"] = "scanning";
                                break;
                            case TEZI_INSTALLING:
                                tezi_status["status"] = "installing";
                                break;
                            case TEZI_INSTALLED:
                                tezi_status["status"] = "installed";
                                break;
                            case TEZI_ABORTED:
                                tezi_status["status"] = "aborted";
                                break;
                            case TEZI_FAILED:
                                tezi_status["status"] = "failed";
                                break;
                            default:
                                tezi_status["status"] = "unkown";
                                break;
                        }
                        returnValue = Json::serialize(QVariant(tezi_status));
                    } else {
                        QString info("Toradex Easy Installer " VERSION_NUMBER " HTTP API");
                        returnValue = info.toUtf8();
                    }
                }
                break;
                case EHTTP_POST: {
                    if (req->url().path() == "/feeds") {
                        QJsonDocument doc = QJsonDocument::fromJson(req->body());
                        if (doc.isNull()) {
                            status = qhttp::ESTATUS_BAD_REQUEST;
                        } else {
                            QJsonObject object = doc.object();
                            if (object.contains("feed_url")) {
                                emit this->newImageUrl(object["feed_url"].toString());
                            }
                        }
                    } else if (req->url().path() == "/images") {
                        QJsonDocument doc = QJsonDocument::fromJson(req->body());
                        if (doc.isNull() || mainWindow->getTeziState() >= TEZI_INSTALLING){
                            status = qhttp::ESTATUS_BAD_REQUEST;
                        } else {
                            QJsonObject object = doc.object();
                            bool acceptAllLicenses = false;
                            if (object.contains("accept_all_licenses"))
                                acceptAllLicenses = object["accept_all_licenses"].toBool();
                            if (object.contains("image_id")) {
                                if (!object["image_id"].isDouble()) {
                                    status = qhttp::ESTATUS_BAD_REQUEST;
                                    break;
                                }
                                int imageId = object["image_id"].toInt();
                                QMutexLocker locker(mainWindow->_imageList->GetMutex());
                                if (mainWindow->_imageList->imageList().count() < imageId) {
                                    status = qhttp::ESTATUS_BAD_REQUEST;
                                }
                                mainWindow->_acceptAllLicenses = acceptAllLicenses;
                                emit this->httpApiInstallImageById(mainWindow->_imageList->imageList()[imageId]);
                            }
                            if (object.contains("image_url")) {
                                mainWindow->_acceptAllLicenses = acceptAllLicenses;
                                emit this->httpApiInstallImageByUrl(object["image_url"].toString(), ImageSource::SOURCE_NETWORK);
                            }
                        }
                    } else {
                        status = qhttp::ESTATUS_BAD_REQUEST;
                    }
                }
                break;
                default:
                    status = qhttp::ESTATUS_BAD_REQUEST;
                break;
            }
            res->setStatusCode(status);
            res->end(returnValue);
        });
    });
    return 1;
}
