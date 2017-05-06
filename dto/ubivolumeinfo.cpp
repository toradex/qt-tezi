#include "ubivolumeinfo.h"
#include "contentinfo.h"

UbiVolumeInfo::UbiVolumeInfo(const QVariantMap &volume, QObject *parent) :
    QObject(parent), _sizeKib(0), _content(NULL)
{
    _name = volume.value("name").toString();
    if (volume.contains("size_kib"))
        _sizeKib = volume.value("size_kib").toInt();

    if (volume.contains("content")) {
        QVariantMap contentm = volume.value("content").toMap();
        _content = new ContentInfo(contentm, this);
    }

}
