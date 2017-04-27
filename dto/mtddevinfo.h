#ifndef MTDDEVINFO_H
#define MTDDEVINFO_H

#include <QObject>
#include <QVariantMap>

class MtdDevContentInfo;
class UbiVolumeInfo;

class MtdDevInfo : public QObject
{
    Q_OBJECT
public:
    explicit MtdDevInfo(const QVariantMap &blockdev, QObject *parent = 0);

    inline QString name()
    {
        return _name;
    }

    inline void setMtdDevice(const QByteArray &mtddevice)
    {
        _mtdDevice = mtddevice;
    }

    inline QByteArray mtdDevice()
    {
        return _mtdDevice;
    }

    inline MtdDevContentInfo *content()
    {
        return _content;
    }

    inline QList<UbiVolumeInfo *> *ubiVolumes()
    {
        return &_ubiVolumes;
    }

protected:

    QByteArray _mtdDevice;
    QString _name;
    MtdDevContentInfo *_content;
    QList<UbiVolumeInfo *> _ubiVolumes;
};

#endif // MTDDEVINFO_H
