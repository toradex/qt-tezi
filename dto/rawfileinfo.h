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

    inline QByteArray ddOptions()
    {
        return _ddOptions;
    }

    inline QStringList productIds()
    {
        return _productIds;
    }

    inline int size()
    {
        return _size;
    }

signals:

public slots:

protected:
    QString _filename;
    QByteArray _ddOptions;
    QStringList _productIds;
    int _size;
};

#endif // RAWFILEINFO_H
