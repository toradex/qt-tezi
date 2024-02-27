#ifndef MULTIIMAGEWRITETHREAD_H
#define MULTIIMAGEWRITETHREAD_H

#include "dto/imageinfo.h"
#include "configblock.h"
#include "moduleinformation.h"
#include <QThread>
#include <QStringList>
#include <QMultiMap>
#include <QVariantList>
#include <QList>

class BlockDevInfo;
class BlockDevPartitionInfo;
class ContentInfo;
class MtdDevInfo;
class RawFileInfo;
class UbiVolumeInfo;
class WinCEImage;

class MultiImageWriteThread : public QThread
{
    Q_OBJECT
public:
    explicit MultiImageWriteThread(ConfigBlock *configBlock, ModuleInformation *moduleInformation, QObject *parent = 0);
    void setImage(const QString &folder, const QString &fileinfo, const QString &baseurl, enum ImageSource source);

    static QByteArray getBlkId(const QString &part, const QString &tag);
    static QByteArray getLabel(const QString &part);
    static QByteArray getUUID(const QString &part);
    static QByteArray getFsType(const QString &part);

    static bool runCommand(const QString &cmd, const QStringList &args, QByteArray &output, const QByteArray &input = nullptr, int msecs = 30000, const QString &workdir = "/");
    bool runScript(QString script, QByteArray &output);
    static bool eraseMtdDevice(const QByteArray &mtddevice, QByteArray &output);
    static bool eraseBlockDevice(const QByteArray &blockdevice, qint64 start, qint64 end, QByteArray &output);

    ImageInfo *getImageInfo() {
        return _image;
    }

protected:
    virtual void run();
    void updateStatus(QString status);
    QList<RawFileInfo *> filterRawFileInfo(QList<RawFileInfo *> *rawFiles);
    bool processBlockDev(BlockDevInfo *blockdev);
    bool processPartitions(BlockDevInfo *blockdev, QList<BlockDevPartitionInfo *> *partitions);
    bool processContent(ContentInfo *fs, QByteArray partdevice);
    bool processFileCopy(const QString &baseurl, const QString &tarball, const QStringList &filelist, bool linuxfs, bool hasacl);
    bool processMtdDev(MtdDevInfo *mtddev);
    bool processMtdContent(ContentInfo *content, QByteArray mtddevice);
    bool processUbi(QList<UbiVolumeInfo *> *volumes, QByteArray mtddevice);
    bool processUbiContent(ContentInfo *contentInfo, QString ubivoldev);
    bool processWinCEImage(WinCEImage *image, QByteArray mtddevice);
    bool mkfs(const QByteArray &device, const QByteArray &fstype = "ext4", const QByteArray &label = "", const QByteArray &mkfsopt = "");
    bool runwritecmd(const QString &cmd, bool checkmd5sum);
    bool pollpipeview();
    bool partclone_restore(const QString &baseurl, const QString &image, const QString &device);
    bool untar(const QString &baseurl, const QString &tarball, const QString &destdir, bool linuxfs, bool hasacl);
    bool copy(const QString &baseurl, const QString &file, const QString &destdir, const QString &md5sum = "");
    bool dd(const QString &baseurl, const QString &device, RawFileInfo *rawFile);
    bool nandflash(const QString &baseurl, const QString &device, RawFileInfo *rawFile);
    bool ubiflash(const QString &baseurl, const QString &device, RawFileInfo *rawFile);
    bool isLabelAvailable(const QByteArray &label);
    void patchConfigTxt();
    QString getUncompressCommand(const QString &file, bool md5sum);
    bool writePartitionTable(const QByteArray &blockdevpath, const QString &tableType, const QMap<int, BlockDevPartitionInfo *> &partitionMap);
    bool isURL(const QString &s);
    bool setBootPartition0(BlockDevInfo *blockdev);

    ImageInfo *_image;
    ConfigBlock *_configBlock;
    ModuleInformation *_moduleInformation;

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
