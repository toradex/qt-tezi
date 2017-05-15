#ifndef UBIVOLUMEINFO_H
#define UBIVOLUMEINFO_H

#include <QVariantMap>

class ContentInfo;

class UbiVolumeInfo : public QObject
{
    Q_OBJECT
public:
    explicit UbiVolumeInfo(const QVariantMap &volume, QObject *parent);

    inline QString name()
    {
        return _name;
    }

    inline qint32 sizeKib()
    {
        return _sizeKib;
    }

    inline QString type()
    {
        return _type;
    }

    inline void setUbiDevice(const QByteArray &ubidevice)
    {
        _ubiDevice = ubidevice;
    }

    inline QByteArray ubiDevice()
    {
        return _ubiDevice;
    }

    inline ContentInfo *content()
    {
        return _content;
    }

protected:

    QByteArray _ubiDevice;
    qint32 _sizeKib;
    QString _name;
    QString _type;
    ContentInfo *_content;
};

#endif // UBIVOLUMEINFO_H
