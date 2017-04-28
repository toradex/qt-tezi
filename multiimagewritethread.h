#ifndef MULTIIMAGEWRITETHREAD_H
#define MULTIIMAGEWRITETHREAD_H

#include "dto/imageinfo.h"
#include "configblock.h"
#include <QThread>
#include <QStringList>
#include <QMultiMap>
#include <QVariantList>
#include <QList>

class BlockDevInfo;
class MtdDevInfo;
class MtdDevContentInfo;
class PartitionInfo;
class BlockDevContentInfo;
class RawFileInfo;
class UbiVolumeInfo;

class MultiImageWriteThread : public QThread
{
    Q_OBJECT
public:
    explicit MultiImageWriteThread(QObject *parent = 0);
    void setImage(const QString &folder, const QString &fileinfo, const QString &baseurl, enum ImageSource source);
    void setConfigBlock(ConfigBlock *configBlock);

    static QByteArray getBlkId(const QString &part, const QString &tag);
    static QByteArray getLabel(const QString &part);
    static QByteArray getUUID(const QString &part);
    static QByteArray getFsType(const QString &part);

    ImageInfo *getImageInfo() {
        return _image;
    }

protected:
    virtual void run();
    bool runScript(QString script, QByteArray &output);
    bool runCommand(QString cmd, QStringList args, QByteArray &output);
    bool eraseMtdDevice(QByteArray mtddevice);
    bool processUbi(QList<UbiVolumeInfo *> *volumes, QByteArray mtddevice);
    bool processBlockDev(BlockDevInfo *blockdev);
    bool processPartitions(BlockDevInfo *blockdev, QList<PartitionInfo *> *partitions);
    bool processContent(BlockDevContentInfo *fs, QByteArray partdevice);
    bool processFileCopy(QString tarball, QStringList filelist);
    bool processMtdDev(MtdDevInfo *mtddev);
    bool processMtdContent(MtdDevContentInfo *content, QByteArray mtddevice);
    bool mkfs(const QByteArray &device, const QByteArray &fstype = "ext4", const QByteArray &label = "", const QByteArray &mkfsopt = "");
    bool runwritecmd(const QString &cmd);
    bool dd(const QString &baseurl, const QString &device, RawFileInfo *rawFile);
    bool partclone_restore(const QString &imagePath, const QString &device);
    bool untar(const QString &tarball);
    bool copy(const QString &baseurl, const QString &file);
    bool isLabelAvailable(const QByteArray &label);
    void patchConfigTxt();
    QString getDescription(const QString &folder, const QString &flavour);
    QString getUncompressCommand(const QString &file);
    bool writePartitionTable(QByteArray blockdevpath, const QMap<int, PartitionInfo *> &partitionMap);
    bool isURL(const QString &s);

    ImageInfo *_image;
    ConfigBlock *_configBlock;

    int _extraSpacePerPartition, _sectorOffset;
    qint64 _bytesWritten;
    QVariantList installed_os;

signals:
    void error(const QString &msg);
    void statusUpdate(const QString &msg);
    void parsedImagesize(qint64 size);
    void imageProgress(qint64 size);
    void completed();
    
public slots:
    
};

#endif // MULTIIMAGEWRITETHREAD_H
