#ifndef IMAGEINFO_H
#define IMAGEINFO_H

/**
 * OS info model
 * Contains the information from os.json, and has a pointer to the partitions
 */

#include <QObject>
#include <QList>

enum ImageSource {
    SOURCE_USB,
    SOURCE_SDCARD,
    SOURCE_NETWORK,
    SOURCE_RNDIS,
};

class BlockDevInfo;
class MtdDevInfo;

class ImageInfo : public QObject
{
    Q_OBJECT
public:

    /* Constructor parses the json files in <folder>, and stores information in local variables */
    explicit ImageInfo(const QString &folder, const QString &infofile, const QString &baseUrl, enum ImageSource source, QObject *parent = 0);

    static bool isNetwork(enum ImageSource source)
    {
        return source == SOURCE_NETWORK || source == SOURCE_RNDIS;
    }

    inline QString folder()
    {
        return _folder;
    }

    inline QString infofile()
    {
        return _infofile;
    }

    inline QString name()
    {
        return _name;
    }

    inline QString description()
    {
        return _description;
    }

    inline QString version()
    {
        return _version;
    }

    inline QString releaseDate()
    {
        return _releaseDate;
    }

    inline QString prepareScript()
    {
        return _prepareScript;
    }

    inline QString wrapupScript()
    {
        return _wrapupScript;
    }

    inline bool isInstaller()
    {
        return _isInstaller;
    }

    inline QList<BlockDevInfo *> *blockdevs()
    {
        return &_blockdevs;
    }

    inline QList<MtdDevInfo *> *mtddevs()
    {
        return &_mtddevs;
    }

    inline QList<QString> supportedProductIds() {
        return _supportedProductIds;
    }

    inline enum ImageSource imageSource() {
        return _imageSource;
    }

    inline QString baseUrl() {
        return _baseUrl;
    }

protected:
    QString _folder, _infofile, _baseUrl;
    QString _name, _description, _version, _releaseDate, _prepareScript, _wrapupScript;
    bool _bootable, _isInstaller;
    QList<BlockDevInfo *> _blockdevs;
    QList<MtdDevInfo *> _mtddevs;
    QList<QString> _supportedProductIds;
    enum ImageSource _imageSource;

};

#endif // IMAGEINFO_H
