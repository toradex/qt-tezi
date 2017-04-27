#include "ubivolumeinfo.h"
#include "mtddevcontentinfo.h"

UbiVolumeInfo::UbiVolumeInfo(const QVariantMap &volume, QObject *parent) :
    QObject(parent), _size(0)
{
    _name = volume.value("name").toString();
    if (volume.contains("size"))
        _size = volume.value("size").toInt();

    if (volume.contains("content")) {
        QVariantMap contentm = volume.value("content").toMap();
        _content = new MtdDevContentInfo(contentm, this);
    }

}
