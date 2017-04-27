#ifndef MTDDEVCONTENTINFO_H
#define MTDDEVCONTENTINFO_H

#include <QObject>
#include <QVariantMap>

class RawFileInfo;

class MtdDevContentInfo : public QObject
{
    Q_OBJECT
public:
    explicit MtdDevContentInfo(const QVariantMap &m, QObject *parent = 0);

    inline QByteArray fsType()
    {
        return _fstype;
    }

    inline QList<RawFileInfo *> *rawFiles()
    {
        return &_rawFiles;
    }

protected:
    QByteArray _fstype;
    QList<RawFileInfo *> _rawFiles;
};

#endif // MTDDEVCONTENTINFO_H
