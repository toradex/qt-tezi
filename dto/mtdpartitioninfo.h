#ifndef MTDPARTITIONINFO_H
#define MTDPARTITIONINFO_H

#include <QByteArray>
#include <QObject>
#include <QVariantMap>

class MtdDevContentInfo;

class MtdPartitionInfo : public QObject
{
    Q_OBJECT
public:
    explicit MtdPartitionInfo(const QVariantMap &m, QObject *parent = 0);

    inline QString name()
    {
        return _name;
    }

    inline MtdDevContentInfo *content()
    {
        return _content;
    }

protected:

    QByteArray _mtdDevice;
    QString _name;
    MtdDevContentInfo *_content;
};

#endif // MTDPARTITIONINFO_H
