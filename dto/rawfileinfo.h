#ifndef RAWFILEINFO_H
#define RAWFILEINFO_H

#include <QObject>
#include <QVariantMap>
#include <QStringList>

class RawFileInfo : public QObject
{
    Q_OBJECT
public:
    explicit RawFileInfo(const QVariantMap &rawfile, QObject *parent = 0);

    inline QString filename()
    {
        return _filename;
    }

    inline QString ddOptions()
    {
        return _ddOptions;
    }

    inline QByteArray nandwriteOptions()
    {
        return _nandwriteOptions;
    }

    inline QStringList productIds()
    {
        return _productIds;
    }

    inline int size()
    {
        return _size;
    }

    inline int offset()
    {
        return _offset;
    }

signals:

public slots:

protected:
    QString _filename;
    QString _ddOptions;
    QByteArray _nandwriteOptions;
    QStringList _productIds;
    int _size;
    qint64 _offset;
};

#endif // RAWFILEINFO_H
