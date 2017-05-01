#ifndef RESOURCEDOWNLOAD_H
#define RESOURCEDOWNLOAD_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class ResourceDownload : public QObject
{
    Q_OBJECT
public:
    explicit ResourceDownload(QNetworkAccessManager *netaccess, const QString &urlstring, const QString &saveAs, int index = 0, QObject *parent = 0);
    int saveToFile();

    inline QString saveAs()
    {
        return _saveAs;
    }
    inline QString urlString()
    {
        return _urlString;
    }
    inline QByteArray data()
    {
        return _data;
    }
    inline int networkError()
    {
        return _networkError;
    }
    inline QString networkErrorString()
    {
        return _networkErrorString;
    }
    inline int httpStatusCode()
    {
        return _httpStatusCode;
    }
    inline int index()
    {
        return _index;
    }
signals:
    void completed();
    void failed();
    void finished();
    void abort();

public slots:

protected slots:
    void downloadRedirectCheck();
    void abortDownload();

private:
    void downloadFile(const QString &urlstring);

    QNetworkAccessManager *_netaccess;
    QNetworkReply *_reply;
    QString _saveAs;
    int _index;
    QString _urlString;
    QByteArray _data;
    int _networkError;
    QString _networkErrorString;
    int _httpStatusCode;
};

#endif // RESOURCEDOWNLOAD_H
