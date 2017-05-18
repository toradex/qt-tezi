#include "mtddevinfo.h"
#include "contentinfo.h"
#include "ubivolumeinfo.h"
#include "winceimage.h"

MtdDevInfo::MtdDevInfo(const QVariantMap &mtddev, QObject *parent) :
    QObject(parent), _content(NULL), _winCEImage(NULL)
{
    _name = mtddev.value("name").toString();

    if (mtddev.contains("content")) {
        QVariantMap content = mtddev.value("content").toMap();
        _content = new ContentInfo(content, this);
    }

    if (mtddev.contains(("ubivolumes"))) {
        QVariantList ubivolumes = mtddev.value("ubivolumes").toList();
        foreach (QVariant uv, ubivolumes) {
            QVariantMap ubivolume = uv.toMap();
            _ubiVolumes.append(new UbiVolumeInfo(ubivolume, this));
        }

    }

    if (mtddev.contains(("winceimage"))) {
        QVariantMap winceimage = mtddev.value("winceimage").toMap();
        _winCEImage = new WinCEImage(winceimage, this);
    }
}

