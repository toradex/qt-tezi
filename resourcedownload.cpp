#include "resourcedownload.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDebug>
#include <QFile>

/*
 * Downloads a resource over http, is able to follow http redirects
 * Author: Stefan Agner
 *
 * See LICENSE.txt for license details
 *
 */
ResourceDownload::ResourceDownload(QNetworkAccessManager *netaccess, const QString &urlstring, const QString &saveAs, QObject *parent) : QObject(parent),
  _netaccess(netaccess), _saveAs(saveAs)
{
    if (saveAs != NULL)
        qDebug() << "Downloading" << urlstring << "to" << saveAs;
    else
        qDebug() << "Downloading" << urlstring;
    downloadFile(urlstring);
}

void ResourceDownload::downloadFile(const QString &urlstring)
{
    _urlString = urlstring;
    QUrl url(urlstring);
    QNetworkRequest request(url);
    QNetworkReply *reply = _netaccess->get(request);
    connect(reply, SIGNAL(finished()), this, SLOT(downloadRedirectCheck()));
}

void ResourceDownload::downloadRedirectCheck()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    int httpstatuscode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString redirectionurl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();

    if (httpstatuscode > 300 && httpstatuscode < 400) {
        qDebug() << "Redirection - Re-trying download from" << redirectionurl;
        downloadFile(redirectionurl);
    } else {
        _httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        _networkError = reply->error();

        if (_networkError != QNetworkReply::NoError) {
            _networkErrorString = reply->errorString();
            emit failed();
        } else if (_httpStatusCode < 200 || _httpStatusCode > 399) {
            emit failed();
        } else {
            _data = reply->readAll();
            emit completed();
        }
        emit finished();
    }

    reply->deleteLater();
}

int ResourceDownload::saveToFile()
{
    QFile f(_saveAs);
    f.open(f.WriteOnly);
    if (f.write(_data) == -1)
    {
        return -1;
    }
    f.close();

    return 0;
}
