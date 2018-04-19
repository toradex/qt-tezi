#ifndef MTDDEVINFO_H
#define MTDDEVINFO_H

#include <QObject>
#include <QVariantMap>

class ContentInfo;
class UbiVolumeInfo;
class WinCEImage;

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

    inline ContentInfo *content()
    {
        return _content;
    }

    inline QList<UbiVolumeInfo *> *ubiVolumes()
    {
        return &_ubiVolumes;
    }

    inline WinCEImage *winCEImage()
    {
        return _winCEImage;
    }

    inline bool erase()
    {
        return _erase;
    }

protected:

    QByteArray _mtdDevice;
    QString _name;
    bool _erase;
    ContentInfo *_content;
    WinCEImage *_winCEImage;
    QList<UbiVolumeInfo *> _ubiVolumes;
};

#endif // MTDDEVINFO_H
