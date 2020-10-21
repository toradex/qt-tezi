#ifndef IMAGELIST_H
#define IMAGELIST_H

#include "imagelistdownload.h"

#include <QObject>
#include <QMutex>

class ImageList : public QObject
{
    Q_OBJECT
private:
    QMutex listMutex;
public:
    explicit ImageList(const QString& toradexProductNumber, QObject *parent = nullptr);
    void addImage(const QVariantMap &image);

    inline QListVariantMap& imageList() {
        return _imageList;
    }

    QMutex* GetMutex(){
        return &this->listMutex;
    }

protected:
    QListVariantMap _imageList;
    const QString _toradexPid8;

    void removeTemporaryFiles(const QVariantMap entry);
    static bool imageSortOrder(const QVariant &v1, const QVariant &v2);

signals:
    void imageListUpdated();
    void foundAutoInstallImage(const QVariantMap &image);

public slots:
    void addImages(QListVariantMap images);
    void removeImagesByBlockdev(const QString blockdev);
    void removeImagesBySource(enum ImageSource source);
};

#endif // IMAGELIST_H
