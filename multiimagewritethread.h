#ifndef MULTIIMAGEWRITETHREAD_H
#define MULTIIMAGEWRITETHREAD_H

#include "osinfo.h"
#include <QThread>
#include <QStringList>
#include <QMultiMap>
#include <QVariantList>
#include <QList>

class OsInfo;
class BlockDevInfo;
class PartitionInfo;
class FileSystemInfo;

class MultiImageWriteThread : public QThread
{
    Q_OBJECT
public:
    explicit MultiImageWriteThread(QObject *parent = 0);
    void setImage(const QString &folder, const QString &fileinfo, const QString &baseurl, enum ImageSource source);

protected:
    virtual void run();
    bool runScript(QString script, QByteArray &output);
    bool processBlockDev(BlockDevInfo *blockdev);
    bool processPartitions(BlockDevInfo *blockdev, QList<PartitionInfo *> *partitions);
    bool processContent(FileSystemInfo *fs, QByteArray partdevice);
    bool mkfs(const QByteArray &device, const QByteArray &fstype = "ext4", const QByteArray &label = "", const QByteArray &mkfsopt = "");
    bool runwritecmd(const QString &cmd);
    bool dd(const QString &imagePath, const QString &device, const QByteArray &dd_options);
    bool partclone_restore(const QString &imagePath, const QString &device);
    bool untar(const QString &tarball);
    bool copy(const QString &baseurl, const QString &file);
    bool isLabelAvailable(const QByteArray &label);
    QByteArray getLabel(const QString part);
    QByteArray getUUID(const QString part);
    void patchConfigTxt();
    QString getDescription(const QString &folder, const QString &flavour);
    QString getUncompressCommand(const QString &file);
    bool writePartitionTable(QByteArray blockdevpath, const QMap<int, PartitionInfo *> &partitionMap);
    bool isURL(const QString &s);

    OsInfo *_image;

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
