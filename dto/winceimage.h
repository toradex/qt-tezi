#ifndef WINCEIMAGE_H
#define WINCEIMAGE_H

#include <QObject>
#include <QVariantMap>

class WinCEImage : public QObject
{
    Q_OBJECT
public:
    explicit WinCEImage(const QVariantMap &winceimage, QObject *parent);

    inline QString imageFilename()
    {
        return _imageFilename;
    }

    inline int nonFsSize()
    {
        return _nonFsSize;
    }

    inline int size()
    {
        return _size;
    }
signals:

public slots:

protected:
    QString _imageFilename;
    int _nonFsSize, _size;
};

#endif // WINCEIMAGE_H
