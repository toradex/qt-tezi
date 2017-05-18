#include "winceimage.h"


WinCEImage::WinCEImage(const QVariantMap &volume, QObject *parent) :
    QObject(parent)
{
    _imageFilename = volume.value("image_filename").toString();
    _nonFsSize = volume.value("nonfs_size", 64).toInt();
    _size = volume.value("size", 0).toInt();
}
