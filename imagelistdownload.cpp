#include "imagelistdownload.h"
#include "resourcedownload.h"
#include "json.h"
#include "config.h"
#include "util.h"
#include "mediapollthread.h"

#include <QDir>
#include <QDebug>
#include <QMessageBox>

/*
 * Downloads a complete image list file using multiple requests
 *
 * Initial author: Stefan Agner
 */
ImageListDownload::ImageListDownload(const QString &url, ImageSource imageSource, int index,
    QNetworkAccessManager *netaccess, QObject *parent) : QObject(parent),
    _imageListUrl(url), _netaccess(netaccess), _parent(parent), _numDownloads(0), _feedindex(index),
    _imageSource(imageSource), _forceAutoinstall(false)
{
    qDebug() << "Downloading image list from " << url;

    _numDownloads++;
    ResourceDownload *rd = new ResourceDownload(_netaccess, url, NULL);
    connect(rd, SIGNAL(failed()), this, SLOT(downloadListJsonFailed()));
    connect(rd, SIGNAL(completed()), this, SLOT(downloadListJsonCompleted()));
    connect(rd, SIGNAL(finished()), this, SLOT(downloadFinished()));
    connect(_parent, SIGNAL(abortAllDownloads()), this, SLOT(downloadAborted()));
    connect(_parent, SIGNAL(abortAllDownloads()), rd, SLOT(abortDownload()));
}

ImageListDownload::ImageListDownload(const QString &url, ImageSource imageSource,
                                     QNetworkAccessManager *netaccess,
                                     QObject *parent) : QObject(parent),
    _imageListUrl(url), _netaccess(netaccess), _parent(parent), _numDownloads(0),
    _imageSource(imageSource), _forceAutoinstall(true)
{
    qDebug() << "Downloading single image description from " << url;

    _numDownloads++;
    ResourceDownload *rd = new ResourceDownload(_netaccess, url, NULL, -1);
    connect(rd, SIGNAL(failed()), this, SLOT(downloadImageJsonFailed()));
    connect(rd, SIGNAL(completed()), this, SLOT(downloadImageJsonCompleted()));
    connect(rd, SIGNAL(finished()), this, SLOT(downloadFinished()));
    connect(_parent, SIGNAL(abortAllDownloads()), this, SLOT(downloadAborted()));
    connect(_parent, SIGNAL(abortAllDownloads()), rd, SLOT(abortDownload()));
}

void ImageListDownload::downloadListJsonCompleted()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());
    int index = 0;

    QVariant json = Json::parse(rd->data());
    rd->deleteLater();
    if (json.isNull()) {
        emit error(tr("Error parsing list JSON downloaded from server"));
        return;
    }

    QVariantMap map = json.toMap();
    if (map.value("config_format").toInt() > IMAGE_LIST_CONFIG_FORMAT) {
        emit error(tr("Image list config format not supported!"));
        return;
    }

    QVariantList list = map.value("images").toList();
    QString sourceurlpath = getUrlPath(rd->urlString());

    foreach (QVariant image, list)
    {
        QString url = image.toString();

        // Relative URL...
        if (!url.startsWith("http"))
            url = sourceurlpath + url;

        _numDownloads++;
        ResourceDownload *rd = new ResourceDownload(_netaccess, url, NULL, index);
        connect(rd, SIGNAL(failed()), this, SLOT(downloadImageJsonFailed()));
        connect(rd, SIGNAL(completed()), this, SLOT(downloadImageJsonCompleted()));
        connect(rd, SIGNAL(finished()), this, SLOT(downloadFinished()));
        connect(_parent, SIGNAL(abortAllDownloads()), rd, SLOT(abortDownload()));
        index++;
    }
}

void ImageListDownload::downloadImageJsonCompleted()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    QByteArray json = rd->data();
    QVariantMap imagemap = Json::parse(json).toMap();

    int index = rd->index();
    QString baseurl = getUrlPath(rd->urlString());
    QString basename = getUrlTopDir(baseurl);
    QString filename = getUrlImageFileName(rd->urlString());
    QString folder = "/var/volatile/" + basename;
    QDir d;
    while (d.exists(folder))
        folder += '_';
    d.mkpath(folder);
    imagemap["folder"] = folder;
    imagemap["index"] = index;

    if (!imagemap.contains("nominal_size"))
        imagemap["nominal_size"] = MediaPollThread::calculateNominalSize(imagemap);

    QFile imageinfo(folder + QDir::separator() + filename);
    imageinfo.open(QIODevice::WriteOnly | QIODevice::Text);
    imageinfo.write(json);
    imageinfo.close();
    imagemap["image_info"] = filename;
    imagemap["baseurl"] = baseurl;
    imagemap["source"] = _imageSource;
    imagemap["feedindex"] = _feedindex;
    if (_forceAutoinstall)
        imagemap["autoinstall"] = true;
    _netImages.append(imagemap);

    QString icon = imagemap.value("icon").toString();
    if (!icon.isNull()) {
        QString iconurl = icon;
        if (!icon.startsWith("http"))
            iconurl = imagemap["baseurl"].toString() + icon;

        _numDownloads++;
        ResourceDownload *rd = new ResourceDownload(_netaccess, iconurl, icon, index);
        connect(rd, SIGNAL(failed()), this, SLOT(downloadIconFailed()));
        connect(rd, SIGNAL(completed()), this, SLOT(downloadIconCompleted()));
        connect(rd, SIGNAL(finished()), this, SLOT(downloadFinished()));
        connect(_parent, SIGNAL(abortAllDownloads()), rd, SLOT(abortDownload()));
    }
}

bool ImageListDownload::orderByIndex(const QVariantMap &m1, const QVariantMap &m2)
{
    QVariant v1 = m1.value("index");
    QVariant v2 = m2.value("index");
    if (v1.isNull() || v2.isNull())
        return false;

    int i1 = v1.value<int>();
    int i2 = v2.value<int>();
    return i1 < i2;
}

void ImageListDownload::downloadFinished()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    // We can rely on this to get always called, no matter whether there
    // has been an error or not...
    _numDownloads--;

    if (!_numDownloads)
    {
        // Add all images at once, in the right order...
        qSort(_netImages.begin(), _netImages.end(), orderByIndex);
        emit newImagesToAdd(_netImages);
        emit finished();

        this->deleteLater();
    }

    rd->deleteLater();
}

void ImageListDownload::downloadAborted()
{
    emit finished();
    this->deleteLater();
}

void ImageListDownload::downloadListJsonFailed()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    /* Emit error if image_list file download failed */
    if (rd->networkError() != QNetworkReply::NoError) {
        emit error(tr("Error downloading image list: %1\nURL: %2\nPlease try again.").arg(rd->networkErrorString(), rd->urlString()));
    } else {
        emit error(tr("Error downloading image list: HTTP status code %1\nURL: %2").arg(rd->httpStatusCode() + "", rd->urlString()));
    }
}

void ImageListDownload::downloadImageJsonFailed()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    /* Silently ignore if a single image in image_list is missing... */
    if (rd->networkError() != QNetworkReply::NoError) {
        qDebug() << "Getting image JSON failed:" << rd->networkErrorString();
    } else {
        qDebug() << "Getting image JSON failed: HTTP status code" << rd->httpStatusCode();
    }
}

void ImageListDownload::downloadIconCompleted()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    for (QList<QVariantMap>::iterator i = _netImages.begin(); i != _netImages.end(); ++i)
    {
        // Assign icon...
        if (i->value("index") == rd->index())
            i->insert("iconimage", rd->data());
    }
}

void ImageListDownload::downloadIconFailed()
{
    ResourceDownload *rd = qobject_cast<ResourceDownload *>(sender());

    qDebug() << "Error downloading icon" << rd->urlString();
}
