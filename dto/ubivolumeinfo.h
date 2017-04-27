#ifndef UBIVOLUMEINFO_H
#define UBIVOLUMEINFO_H

#include <QVariantMap>

class MtdDevContentInfo;

class UbiVolumeInfo : public QObject
{
    Q_OBJECT
public:
    explicit UbiVolumeInfo(const QVariantMap &volume, QObject *parent);

    inline QString name()
    {
        return _name;
    }

    inline qint32 size()
    {
        return _size;
    }

    inline void setUbiDevice(const QByteArray &ubidevice)
    {
        _ubiDevice = ubidevice;
    }

    inline QByteArray ubiDevice()
    {
        return _ubiDevice;
    }

    inline MtdDevContentInfo *content()
    {
        return _content;
    }

protected:

    QByteArray _ubiDevice;
    qint32 _size; //KiB
    QString _name;
    MtdDevContentInfo *_content;
};

#endif // UBIVOLUMEINFO_H
