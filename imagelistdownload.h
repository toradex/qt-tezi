#ifndef IMAGELISTDOWNLOAD_H
#define IMAGELISTDOWNLOAD_H

#include "dto/imageinfo.h"

#include <QObject>
#include <QVariantMap>

typedef QList<QVariantMap> QListVariantMap;

class QNetworkAccessManager;

class ImageListDownload : public QObject
{
    Q_OBJECT
public:
    explicit ImageListDownload(const QString &url, int index, QNetworkAccessManager *netaccess, QObject *parent);

    inline int index()
    {
        return _index;
    }

signals:
    void newImagesToAdd(const QListVariantMap images);
    void finished();
    void error(const QString &msg);

public slots:

protected slots:

    void downloadListJsonCompleted();
    void downloadListJsonFailed();
    void downloadImageJsonCompleted();
    void downloadImageJsonFailed();
    void downloadIconCompleted();
    void downloadIconFailed();
    void downloadFinished();

protected:
    static bool orderByIndex(const QVariantMap &m1, const QVariantMap &m2);
    const QString _imageListUrl;
    QNetworkAccessManager *_netaccess;
    QObject *_parent;
    QList<QVariantMap> _netImages;
    int _numDownloads, _index;
};

#endif // IMAGELISTDOWNLOAD_H
