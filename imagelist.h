#ifndef IMAGELIST_H
#define IMAGELIST_H

#include "imagelistdownload.h"

#include <QObject>

class ImageList : public QObject
{
    Q_OBJECT
public:
    explicit ImageList(const QString& toradexProductNumber, QObject *parent = nullptr);
    void addImage(const QVariantMap &image);

    inline QListVariantMap& imageList() {
        return _imageList;
    }

protected:
    QListVariantMap _imageList;
    const QString _toradexProductNumber;

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
