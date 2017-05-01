#include "resourcedownload.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QDebug>
#include <QFile>

/*
 * Downloads a resource over http, is able to follow http redirects
 * Author: Stefan Agner
 *
 * See LICENSE.txt for license details
 *
 */
ResourceDownload::ResourceDownload(QNetworkAccessManager *netaccess, const QString &urlstring, const QString &saveAs, int index, QObject *parent) : QObject(parent),
  _netaccess(netaccess), _saveAs(saveAs), _index(index), _networkError(QNetworkReply::NoError)
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
    _reply = _netaccess->get(request);
    connect(_reply, SIGNAL(finished()), this, SLOT(downloadRedirectCheck()));
}

void ResourceDownload::abortDownload()
{
    qDebug() << "Emit abort...";
    _reply->abort();
}

void ResourceDownload::downloadRedirectCheck()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    int httpstatuscode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString redirectionurl = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toString();

    _networkError = reply->error();
    if (_networkError == QNetworkReply::OperationCanceledError) {
        emit finished();
    } else if (_networkError != QNetworkReply::NoError) {
        _networkErrorString = reply->errorString();
        emit failed();
        emit finished();
    } else if (httpstatuscode > 300 && httpstatuscode < 400) {
        qDebug() << "Redirection - Re-trying download from" << redirectionurl;
        downloadFile(redirectionurl);
    } else {
        _httpStatusCode = httpstatuscode;

        if (_httpStatusCode < 200 || _httpStatusCode > 399) {
            emit failed();
        } else {
            _data = reply->readAll();
            emit completed();
        }
        // This gets always called, no matter wheter a download succeeded or failed.
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
