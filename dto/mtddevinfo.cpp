#include "mtddevinfo.h"
#include "mtddevcontentinfo.h"
#include "ubivolumeinfo.h"

MtdDevInfo::MtdDevInfo(const QVariantMap &mtddev, QObject *parent) :
    QObject(parent), _content(NULL)
{
    _name = mtddev.value("name").toString();

    if (mtddev.contains("content")) {
        QVariantMap content = mtddev.value("content").toMap();
        _content = new MtdDevContentInfo(content, this);
    }

    if (mtddev.contains(("ubivolumes"))) {
        QVariantList ubivolumes = mtddev.value("ubivolumes").toList();
        foreach (QVariant uv, ubivolumes) {
            QVariantMap ubivolume = uv.toMap();
            _ubiVolumes.append(new UbiVolumeInfo(ubivolume, this));
        }

    }
}

