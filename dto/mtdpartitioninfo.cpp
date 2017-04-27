#include "mtdpartitioninfo.h"
#include "mtddevcontentinfo.h"

MtdPartitionInfo::MtdPartitionInfo(const QVariantMap &part, QObject *parent) :
    QObject(parent), _content(NULL)
{
    _name = part.value("name").toString();

    if (part.contains("content")) {
        QVariantMap contentm = part.value("content").toMap();
        _content = new MtdDevContentInfo(contentm, this);
    }
}

