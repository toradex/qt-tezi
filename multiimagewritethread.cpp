#include "multiimagewritethread.h"
#include "dto/blockdevinfo.h"
#include "dto/blockdevpartitioninfo.h"
#include "dto/contentinfo.h"
#include "dto/mtddevinfo.h"
#include "dto/rawfileinfo.h"
#include "dto/ubivolumeinfo.h"
#include "dto/winceimage.h"
#include "config.h"
#include "json.h"
#include "util.h"
#include "mtdnamedevicetranslator.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QTime>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <QtEndian>

MultiImageWriteThread::MultiImageWriteThread(ConfigBlock *configBlock, ModuleInformation *moduleInformation, QObject *parent) :
    QThread(parent), _configBlock(configBlock), _moduleInformation(moduleInformation),  _extraSpacePerPartition(0), _bytesWritten(0)
{
    QDir dir;

    if (!dir.exists(TEMP_MOUNT_FOLDER))
        dir.mkpath(TEMP_MOUNT_FOLDER);

    if (!QFile::exists(PIPEVIEWER_NAMEDPIPE))
        makeFifo(PIPEVIEWER_NAMEDPIPE);
    if (!QFile::exists(MD5SUM_NAMEDPIPE))
        makeFifo(MD5SUM_NAMEDPIPE);
}

void MultiImageWriteThread::setImage(const QString &folder, const QString &infofile, const QString &baseurl, enum ImageSource source)
{
    /* Make sure that baseurl is empty for local image sources, we rely on it */
    if (ImageInfo::isNetwork(source))
        _image = new ImageInfo(folder, infofile, baseurl, source, this);
    else
        _image = new ImageInfo(folder, infofile, QString(), source, this);
}

void MultiImageWriteThread::updateStatus(QString status)
{
    emit statusUpdate(QString("%1: %2").arg(_image->name(), status));
}

void MultiImageWriteThread::run()
{
    QList<BlockDevInfo *> *blockdevs = _image->blockdevs();
    QList<MtdDevInfo *> *mtddevs = _image->mtddevs();

    qDebug() << "Processing Image:" << _image->name();

    if (!_configBlock) {
        emit error(tr("No valid config block available"));
        return;
    }

    /* Run prepare script */
    if (!_image->prepareScript().isEmpty()) {
        QString prepareScript = _image->folder() + "/" + _image->prepareScript();
        if (!QFile::exists(prepareScript)) {
            emit error(tr("Prepare script '%1' does not exist").arg(prepareScript));
            return;
        }

        QByteArray output;
        if (!runScript(prepareScript, output)) {
            emit error(tr("Error executing prepare script") + "\n" + output);
            return;
        }
    }

    foreach (BlockDevInfo *blockdev, *blockdevs) {
        qDebug() << "Processing blockdev: " << blockdev->name();

        QByteArray blockDevice = "/dev/" + blockdev->name().toAscii();
        if (!QFile::exists(blockDevice)) {
            emit error(tr("Block device '%1' does not exist").arg(QString(blockDevice)));
            return;
        }
        blockdev->setBlockDevice(blockDevice);

        if (!processBlockDev(blockdev))
            return;
    }

    if (!mtddevs->empty()) {
        MtdNameDeviceTranslator mtdNameDev;

        foreach (MtdDevInfo *mtddev, *mtddevs) {
            QString mtdname = mtddev->name();
            QString mtddevname = mtdNameDev.translate(mtdname);
            qDebug() << "Processing mtddev: " << mtddevname;

            QByteArray mtdDevice = "/dev/" + mtddevname.toAscii();
            if (!QFile::exists(mtdDevice)) {
                emit error(tr("Mtd device '%1' does not exist").arg(QString(mtdDevice)));
                return;
            }
            mtddev->setMtdDevice(mtdDevice);

            if (!processMtdDev(mtddev))
                return;
        }
    }

    /* Write Config Block after flashing (e.g. migration/restore after partition erase) */
    if (_configBlock->needsWrite) {
        switch (_moduleInformation->storageClass()) {
        case ModuleInformation::StorageClass::Block:
            _configBlock->writeToBlockdev(_moduleInformation->configBlockPartition(), _moduleInformation->configBlockOffset());
            break;
        case ModuleInformation::StorageClass::Mtd:
            _configBlock->writeToMtddev(_moduleInformation->configBlockPartition(), _moduleInformation->configBlockOffset());
            break;
        }
        qDebug() << "Config Block written to " << _moduleInformation->configBlockPartition();
        _configBlock->needsWrite = false;
    }

    /* Run wrap-up script */
    if (!_image->wrapupScript().isEmpty()) {
        QString wrapupScript = _image->folder() + "/" + _image->wrapupScript();
        if (!QFile::exists(wrapupScript)) {
            emit error(tr("Wrap-up script '%1' does not exist").arg(wrapupScript));
            return;
        }

        QByteArray output;
        if (!runScript(wrapupScript, output)) {
            emit error(tr("Error executing wrap-up script") + "\n" + output);
            return;
        }
    }

    emit statusUpdate(tr("Finish writing (syncing)"));
    sync();

    emit completed();
    qDebug() << "Write Thread finished";
}

bool MultiImageWriteThread::runScript(QString script, QByteArray &output)
{
    QProcess p;
    QProcessEnvironment env;
    QStringList cmd(script);

    /* $1: product id */
    cmd.append(_configBlock->getProductNumber());

    /* $2: board rev */
    cmd.append(_configBlock->getBoardRev());

    /* $3: serial */
    cmd.append(_configBlock->getSerialNumber());

    /* $4: image folder */
    cmd.append(_image->folder());

    env.insert("PATH", "/bin:/usr/bin:/sbin:/usr/sbin");

    qDebug() << "Executing:" << "/bin/sh" << cmd;
    qDebug() << "Env:" << env.toStringList();

    p.setProcessChannelMode(QProcess::MergedChannels);
    p.setProcessEnvironment(env);
    p.setWorkingDirectory(_image->folder());
    p.start("/bin/sh", cmd);

    p.waitForFinished(30000);

    output = p.readAll();
    qDebug() << "Output:" << output;
    qDebug() << "Finished with exit code:" << p.exitCode();
    return p.exitCode() == 0;
}

bool MultiImageWriteThread::runCommand(const QString &cmd, const QStringList &args, QByteArray &output, int msecs, const QString &workdir)
{
    QProcess p;

    qDebug() << "Running Command: " << cmd << args;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.setWorkingDirectory(workdir);
    p.start(cmd, args);

    p.waitForFinished(msecs);

    output = p.readAll();
    if (p.exitCode() > 0)
        qDebug() << "Output:" << output;
    qDebug() << "Finished with exit code:" << p.exitCode();
    return p.exitCode() == 0;
}

bool MultiImageWriteThread::processMtdDev(MtdDevInfo *mtddev)
{
    QByteArray device = mtddev->mtdDevice();
    qint64 totaluncompressedsize = 0;

    ContentInfo *content = mtddev->content();
    if (content != NULL) {
        QList<RawFileInfo *> rawFiles = filterRawFileInfo(content->rawFiles());
        foreach (RawFileInfo *rawFile, rawFiles)
            totaluncompressedsize += rawFile->size();
    }

    QList<UbiVolumeInfo *> *volumes = mtddev->ubiVolumes();
    if (volumes->length() > 0) {
        foreach(UbiVolumeInfo *volume, *volumes) {
            if (volume->content() != NULL) {
                totaluncompressedsize += volume->content()->uncompressedSize();
                QList<RawFileInfo *> rawFiles = filterRawFileInfo(volume->content()->rawFiles());
                foreach (RawFileInfo *rawFile, rawFiles)
                    totaluncompressedsize += rawFile->size();
            }
        }
    }

    WinCEImage *winceimage = mtddev->winCEImage();
    if (winceimage != NULL)
        totaluncompressedsize += winceimage->size();

    emit parsedImagesize(totaluncompressedsize * 1024 * 1024);

    /* Erase partition if explicit erase is requested or raw content is specified */
    if (mtddev->erase() || content != NULL) {
        QByteArray output;
        updateStatus(tr("Erasing partition"));

        if (!eraseMtdDevice(device, output)) {
            emit error(tr("Erasing flash failed") + "\n" + output);
            return false;
        }
    }

    if (content != NULL) {
        if (!processMtdContent(content, device))
            return false;
    }

    if (volumes->length() > 0) {
        if (!processUbi(volumes, device))
            return false;
    }

    if (winceimage != NULL) {
        if (!processWinCEImage(winceimage, device))
            return false;
    }

    return true;
}

bool MultiImageWriteThread::processBlockDev(BlockDevInfo *blockdev)
{
    /* Make sure block device is writeable */
    disableBlockDevForceRo(blockdev->name());

    if (blockdev->erase()) {
        updateStatus(tr("Erasing partition"));

        /*
         * We are erasing the partition which contains the config block! Make sure the config block gets
         * written at the end of the flashing process
         */
        if (blockdev->name() == _moduleInformation->configBlockPartition())
                _configBlock->needsWrite = true;

        QByteArray output;
        if (!eraseBlockDevice(blockdev->blockDevice(), 0, 0, output)) {
            emit error(tr("Discarding content on device %1 failed").arg(blockdev->name()) + "\n" + output);
            return false;
        }
    }

    QList<BlockDevPartitionInfo *> *partitions = blockdev->partitions();
    if (!partitions->isEmpty())
    {
        /* Writing partition table to this blockdev */
        if (!processPartitions(blockdev, partitions))
            return false;
    }

    /* Writing raw content ignoring parition layout to this blockdev */
    ContentInfo *content = blockdev->content();
    if (content != NULL) {
        QByteArray device = blockdev->blockDevice();
        if (!processContent(content, device))
            return false;
    }

    return true;
}

bool MultiImageWriteThread::processPartitions(BlockDevInfo *blockdev, QList<BlockDevPartitionInfo *> *partitions)
{
    int totalnominalsize = 0, totaluncompressedsize = 0, numparts = 0, numexpandparts = 0;

    /* Calculate space requirements, and check special requirements */
    int totalSectors = getFileContents("/sys/class/block/" + blockdev->name() + "/size").trimmed().toULongLong();

    /* We need PARTITION_ALIGNMENT sectors at the begining for MBR and general alignment (currently 4MiB) */
    int availableMB = (totalSectors - PARTITION_ALIGNMENT) / 2048;

    /* key: partition number, value: partition information */
    QMap<int, BlockDevPartitionInfo *> partitionMap;

    foreach (BlockDevPartitionInfo *partition, *partitions)
    {
        qDebug() << "Partition:" << numparts << "Type:" << partition->partitionType();
        numparts++;
        if ( partition->wantMaximised() )
            numexpandparts++;
        totalnominalsize += partition->partitionSizeNominal();
        ContentInfo *content = partition->content();
        if (content != NULL)
            totaluncompressedsize += content->uncompressedSize();

        int reqPart = partition->requiresPartitionNumber();
        if (reqPart)
        {
            if (partitionMap.contains(reqPart))
            {
                emit error(tr("More than one content requires partition number %1").arg(reqPart));
                return false;
            }

            partition->setPartitionDevice(blockdev->blockDevice() + "p" + QByteArray::number(reqPart));
            partitionMap.insert(reqPart, partition);
        }

        /* Maximum overhead per partition for alignment */
        if (partition->wantMaximised() || (partition->partitionSizeNominal()*2048) % PARTITION_ALIGNMENT != 0)
            totalnominalsize += PARTITION_ALIGNMENT/2048;
    }

    if (numexpandparts)
    {
        /* Extra spare space available for partitions that want to be expanded */
        _extraSpacePerPartition = (availableMB-totalnominalsize)/numexpandparts;
    }

    emit parsedImagesize(qint64(totaluncompressedsize)*1024*1024);

    if (totalnominalsize > availableMB)
    {
        emit error(tr("Not enough disk space. Need %1 MB, got %2 MB").arg(QString::number(totalnominalsize), QString::number(availableMB)));
        return false;
    }

    /* Assign logical partition numbers to partitions that did not reserve a special number */
    int pnr = 1;
    foreach (BlockDevPartitionInfo *partition, *(blockdev->partitions()))
    {
        if (!partition->requiresPartitionNumber())
        {
            while (partitionMap.contains(pnr))
                pnr++;

            partitionMap.insert(pnr, partition);
            partition->setPartitionDevice(blockdev->blockDevice() + "p" + QByteArray::number(pnr));
        }
    }

    /* Set partition starting sectors and sizes. */
    int offset = PARTITION_ALIGNMENT;
    QList<BlockDevPartitionInfo *> partitionList = partitionMap.values();
    foreach (BlockDevPartitionInfo *p, partitionList)
    {
        if (p->offset()) /* OS wants its partition at a fixed offset */
        {
            if (p->offset() <= offset)
            {
                emit error(tr("Fixed partition offset too low"));
                return false;
            }

            offset = p->offset();
        }
        else
        {
            /* Adhere to partition alignment */
            if (offset % PARTITION_ALIGNMENT != 0)
            {
                    offset += PARTITION_ALIGNMENT-(offset % PARTITION_ALIGNMENT);
            }

            p->setOffset(offset);
        }

        int partsizeMB = p->partitionSizeNominal();
        if ( p->wantMaximised() )
            partsizeMB += _extraSpacePerPartition;
        int partsizeSectors = partsizeMB * 2048;

        if (p == partitionList.last())
        {
            /* Let last partition have any remaining space that we couldn't divide evenly */
            int spaceleft = totalSectors - offset - partsizeSectors;

            if (spaceleft > 0 && p->wantMaximised())
            {
                partsizeSectors += spaceleft;
            }
        }
        else
        {
            if (p->wantMaximised() && partsizeSectors % PARTITION_ALIGNMENT != 0)
            {
                /* Enlarge partition to close gap to next partition */
                partsizeSectors += PARTITION_ALIGNMENT - (partsizeSectors % PARTITION_ALIGNMENT);
            }
        }

        p->setPartitionSizeSectors(partsizeSectors);
        offset += partsizeSectors;
    }

    emit statusUpdate(tr("Writing partition table"));
    if (!writePartitionTable(blockdev->blockDevice(), partitionMap))
        return false;

    /* Zero out first sector of partitions, to make sure to get rid of previous file system (label) */
    emit statusUpdate(tr("Zero'ing start of each partition"));
    foreach (BlockDevPartitionInfo *p, partitionMap.values())
    {
        if (p->partitionSizeSectors())
            QProcess::execute("/bin/dd count=1 bs=512 if=/dev/zero of="+p->partitionDevice());
    }

    /* Install each partition */
    foreach (BlockDevPartitionInfo *p, *partitions)
    {
        ContentInfo *content = p->content();
        if (content != NULL) {
            QByteArray partdevice = p->partitionDevice();
            if (!processContent(content, partdevice))
                return false;
        }
    }

    return true;
}


bool MultiImageWriteThread::writePartitionTable(QByteArray blockdevpath, const QMap<int, BlockDevPartitionInfo *> &partitionMap)
{
    /* Write partition table using sfdisk */
    QByteArray partitionTable;
    for (int i=1; i <= partitionMap.keys().last(); i++)
    {
        if (partitionMap.contains(i))
        {
            BlockDevPartitionInfo *p = partitionMap.value(i);

            partitionTable += QByteArray::number(p->offset())+","+QByteArray::number(p->partitionSizeSectors())+","+p->partitionType();
            if (p->active())
                partitionTable += " *";
            partitionTable += "\n";
        }
        else
        {
            partitionTable += "0,0\n";
        }
    }

    qDebug() << "New partition table:";
    qDebug() << partitionTable;

    /* Let sfdisk write a proper partition table */
    QProcess proc;
    proc.setProcessChannelMode(proc.MergedChannels);
    proc.start("/usr/sbin/sfdisk -uS --label dos " + blockdevpath);
    proc.write(partitionTable);
    proc.closeWriteChannel();
    proc.waitForFinished(-1);

    QByteArray output = proc.readAll();
    qDebug() << "sfdisk done, output:" << output;
    if (proc.exitCode() != 0)
    {
        emit error(tr("Error creating partition table") + "\n" + output);
        return false;
    }

    for (int i=1; i <= partitionMap.keys().last(); i++) {
        if (partitionMap.contains(i))
        {
            BlockDevPartitionInfo *p = partitionMap.value(i);
            while (!QFile::exists(p->partitionDevice()))
                QThread::msleep(10);
        }
    }

    return true;
}

bool MultiImageWriteThread::eraseMtdDevice(const QByteArray &mtddevice, QByteArray &output)
{
    QStringList eraseargs;
    eraseargs << "--quiet" << mtddevice << "0" << "0";

    return runCommand("/usr/sbin/flash_erase", eraseargs, output, -1);
}

bool MultiImageWriteThread::eraseBlockDevice(const QByteArray &blockdevice, qint64 start, qint64 end, QByteArray &output)
{
    QStringList args;

    /* If start and end is zero, blkdiscard will discard the whole eMMC */
    if (start)
        args.append(QString("-o %1").arg(start));
    if (end)
        args.append(QString("-l %1").arg(end));
    args.append(blockdevice);


    return runCommand("/usr/sbin/blkdiscard", args, output, -1);
}

bool MultiImageWriteThread::processUbiContent(ContentInfo *contentInfo, QString ubivoldev)
{
    bool result = true;
    QString fstype = contentInfo->fsType();
    QByteArray output;

    if (fstype == "raw") {
        updateStatus(tr("Writing raw files"));
        QList<RawFileInfo *> rawFiles = filterRawFileInfo(contentInfo->rawFiles());

        if (rawFiles.count() > 1)
            qDebug() << "Warning: UBI volumes support only one raw file per partition";

        /* Typically Kernel/Device Tree */
        if (!ubiflash(_image->baseUrl(), ubivoldev, rawFiles.first()))
            return false;
    } else if(fstype == "ubifs") {
        /* Typically the rootfs */
        updateStatus(tr("Creating filesystem (%2)").arg(QString(fstype)));
        QStringList mkfsargs;
        mkfsargs << ubivoldev;
        runCommand("/usr/sbin/mkfs.ubifs", mkfsargs, output);

        QStringList mountargs;
        mountargs << "-t" << "ubifs" << ubivoldev << TEMP_MOUNT_FOLDER;

        if (!runCommand("mount", mountargs, output))
        {
            emit error(tr("Error mounting file system") + "\n" + output);
            return false;
        }

        QString tarball = contentInfo->filename();
        QStringList filelist = contentInfo->filelist();
        result = processFileCopy(_image->baseUrl(), tarball, filelist);

        QProcess::execute("umount " TEMP_MOUNT_FOLDER);
    }

    return result;
}

bool MultiImageWriteThread::processUbi(QList<UbiVolumeInfo *> *volumes, QByteArray mtddevice)
{
    bool result = true;
    QByteArray output;
    QStringList ubiformatargs;
    ubiformatargs << mtddevice << "-q" << "-y";

    if (!runCommand("/usr/sbin/ubiformat", ubiformatargs, output)) {
        emit error(tr("Formatting UBI partition failed!") + "\n" + output);
        return false;
    }

    QStringList ubiattachargs;
    ubiattachargs << "-p" << mtddevice;

    if (!runCommand("/usr/sbin/ubiattach", ubiattachargs, output)) {
        emit error(tr("Attaching UBI failed!") + "\n" + output);
        return false;
    }

    /* At this point UBI should be attached, add volumes */
    int ubivolid = 0;
    foreach (UbiVolumeInfo *ubivol, *volumes) {
        QByteArray output;
        QStringList ubimkvolargs;
        ubimkvolargs << "/dev/ubi0" << "-N" << ubivol->name() << "-n" << QString::number(ubivolid);

        if (ubivol->sizeKib() > 0)
            ubimkvolargs << "-s" << QString("%1KiB").arg(ubivol->sizeKib());
        else
            ubimkvolargs << "-m";

        if (!ubivol->type().isEmpty())
            ubimkvolargs << "-t" << ubivol->type();

        result = runCommand("/usr/sbin/ubimkvol", ubimkvolargs, output);
        if (!result)
        {
            emit error(tr("Creating UBI volume failed!") + "\n" + output);
            break;
        }

        QString ubivoldev = QString("/dev/ubi0_%1").arg(ubivolid);
        if (ubivol->content())
        {
            result = processUbiContent(ubivol->content(), ubivoldev);
            if (!result)
                break;
        }

        ubivolid++;
    }

    runCommand("/usr/sbin/ubidetach", ubiattachargs, output);

    return result;
}

QList<RawFileInfo *> MultiImageWriteThread::filterRawFileInfo(QList<RawFileInfo *> *rawFiles)
{
    QList<RawFileInfo *> newList;
    QString productNumber = _configBlock->getProductNumber();

    foreach (RawFileInfo *rawFile, *rawFiles) {
        /* Only check against product id if specified */
        if (!rawFile->productIds().isEmpty()) {
            if (!rawFile->productIds().contains(productNumber))
                continue;
        }
        newList.append(rawFile);
    }
    return newList;
}

bool MultiImageWriteThread::processWinCEImage(WinCEImage *image, QByteArray mtddevice)
{
    QString getfile;
    const QString& file = image->imageFilename();
    const QString& baseurl = _image->baseUrl();
    if (!baseurl.isEmpty())
        getfile = WGET_COMMAND + baseurl + file;
    else
        getfile = "cat " + file;

    QString uncompresscmd = getUncompressCommand(file, false);

    /* Use pipe viewer to get bytes processed during the command */
    QString cmd = QString("%1 | %2 | flash_wince -n %3 %4 -")
            .arg(getfile, uncompresscmd, QString::number(image->nonFsSize()), mtddevice);

    qDebug() << "Flash WinCE image file" << file;
    return runwritecmd(cmd, false);
}

bool MultiImageWriteThread::processMtdContent(ContentInfo *content, QByteArray mtddevice)
{
    QByteArray fstype = content->fsType();

    if (fstype == "raw")
    {
        updateStatus(tr("Writing raw files"));

        QList<RawFileInfo *> rawFiles = filterRawFileInfo(content->rawFiles());

        foreach(RawFileInfo *rawFile, rawFiles) {
            if (!nandflash(_image->baseUrl(), mtddevice, rawFile))
                return false;
        }
    }
    return true;
}

bool MultiImageWriteThread::processFileCopy(const QString &baseurl, const QString &tarball, const QStringList &filelist)
{
    bool resultfile = true;
    bool resultfilelist = true;

    if (baseurl.isEmpty())
        updateStatus(tr("Extracting file(s)"));
    else
        updateStatus(tr("Downloading and extracting file(s)"));

    if (!tarball.isEmpty())
        resultfile = untar(baseurl, tarball);

    if (!filelist.isEmpty()) {
        foreach(QString file,filelist) {
            if (!copy(baseurl, file)) {
                resultfilelist = false;
                break;
            }
        }
    }

    return resultfile && resultfilelist;
}

bool MultiImageWriteThread::processContent(ContentInfo *content, QByteArray partdevice)
{
    QByteArray fstype   = content->fsType();
    QByteArray mkfsopt  = content->mkfsOptions();
    QByteArray label = content->label();
    QString tarball  = content->filename();
    QStringList filelist  = content->filelist();

    if (label.size() > 15)
    {
        label.clear();
    }
    else if (!isLabelAvailable(label))
    {
        for (int i=0; i<10; i++)
        {
            if (isLabelAvailable(label+QByteArray::number(i)))
            {
                label = label+QByteArray::number(i);
                break;
            }
        }
    }

    if (fstype == "raw")
    {
        updateStatus(tr("Writing raw files"));
        QList<RawFileInfo *> rawFiles = filterRawFileInfo(content->rawFiles());

        foreach (RawFileInfo *rawFile, rawFiles) {
            if (!dd(_image->baseUrl(), partdevice, rawFile))
                return false;
        }
    }
    else if (fstype.startsWith("partclone"))
    {
        emit statusUpdate(tr("Writing partition clone image"));
        if (!partclone_restore(_image->baseUrl(), tarball, partdevice))
            return false;
    }
    else if (fstype != "unformatted")
    {
        emit statusUpdate(tr("Creating filesystem (%2)").arg(QString(fstype)));
        if (!mkfs(partdevice, fstype, label, mkfsopt))
            return false;

        /* If there is no tarball/filelist specified, its an empty partition which is perfectly ok too */
        if (tarball.isEmpty() && filelist.isEmpty())
            return true;

        emit statusUpdate(tr("Mounting file system"));

        QStringList mountargs;
        mountargs << partdevice << TEMP_MOUNT_FOLDER;

        QString mountcmd;
        if (fstype == "ntfs")
            mountcmd = "/sbin/mount.ntfs-3g";
        else
            mountcmd = "mount";

        QByteArray output;
        if (!runCommand(mountcmd, mountargs, output))
        {
            emit error(tr("Error mounting file system") + "\n" + output);
            return false;
        }

        bool resultfilecopy = processFileCopy(_image->baseUrl(), tarball, filelist);

        if (!runCommand("umount", QStringList() << TEMP_MOUNT_FOLDER, output)) {
            emit error(tr("Error unmounting file system") + "\n" + output);
            return false;
        }

        return resultfilecopy;
    }

    return true;
}

bool MultiImageWriteThread::mkfs(const QByteArray &device, const QByteArray &fstype, const QByteArray &label, const QByteArray &mkfsopt)
{
    QString cmd;
    QStringList args;

    if (fstype == "fat" || fstype == "FAT")
    {
        cmd = "/usr/sbin/mkfs.fat";
        args << "-F" << "32";
        if (!label.isEmpty())
            args << "-n" << label;
    }
    else if (fstype == "ext3")
    {
        cmd = "/sbin/mkfs.ext3";
        if (!label.isEmpty())
            args << "-L" << label;
    }
    else if (fstype == "ext4")
    {
        cmd = "/sbin/mkfs.ext4";
        args << "-L" << label;
    }
    else if (fstype == "ntfs")
    {
        cmd = "/sbin/mkfs.ntfs";
        args << "--fast";
        if (!label.isEmpty())
            args << "-L" << label;
    }

    if (!mkfsopt.isEmpty()) {
        foreach (QByteArray arr, mkfsopt.split(' '))
            args << arr;
    }

    args << device;
    QByteArray output;
    if (!runCommand(cmd, args, output, -1))
    {
        emit error(tr("Error creating file system") + "\n" + output);
        return false;
    }
    return true;
}

bool MultiImageWriteThread::isLabelAvailable(const QByteArray &label)
{
    return (QProcess::execute("/sbin/findfs LABEL="+label) != 0);
}

bool MultiImageWriteThread::runwritecmd(const QString &cmd, bool checkmd5sum)
{
    QTime t1;
    QProcess md5sum;
    t1.start();

    if (checkmd5sum) {
        md5sum.start("md5sum", QStringList() << MD5SUM_NAMEDPIPE);
        md5sum.closeWriteChannel();
        md5sum.setReadChannel(QProcess::StandardOutput);
    }

    QProcess p;
    if (!ImageInfo::isNetwork(_image->imageSource()))
        p.setWorkingDirectory(_image->folder());
    QStringList args;
    args << "-o" << "pipefail" << "-c" << cmd;
    qDebug() << "Running Write Command:" << "sh" << args;
    p.start("sh", args);
    p.closeWriteChannel();
    p.setReadChannel(QProcess::StandardError);

    /* Parse pipe viewer output for progress */
    QFile pv(PIPEVIEWER_NAMEDPIPE);
    if (!pv.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit error(tr("Error downloading or writing image")+"\n" + "Could not open pipe viewers named pipe");
        return false;
    }

    qint64 bytes = 0;
    while (true) {
        /* This will block unless the pipe is EOF */
        QByteArray line = pv.readLine();

        if (line == "")
            break;

        bool ok;
        qint64 tmp = line.trimmed().toLongLong(&ok);

        if (ok) {
            bytes = tmp;
            emit imageProgress(_bytesWritten + bytes);
        }
    }

    _bytesWritten += bytes;
    pv.close();

    p.waitForFinished(-1);

    if (checkmd5sum)
        md5sum.waitForFinished(-1);

    if (p.exitCode() != 0)
    {
        emit error(tr("Error downloading or writing image")+"\n" + p.readAll());
        return false;
    }
    qDebug() << "Finished writing after" << (t1.elapsed()/1000.0) << "seconds," << _bytesWritten << "bytes total so far.";

    if (checkmd5sum)
    {
        if (md5sum.exitCode() != 0)
        {
            emit error(tr("Error calculating checksum")+"\n" + md5sum.readAll());
            return false;
        }
        qDebug() << md5sum.readAll().left(32);
    }

    return true;
}

bool MultiImageWriteThread::copy(const QString &baseurl, const QString &file, const QString &md5sum)
{
    QString getfile;
    if (!baseurl.isEmpty())
        getfile += WGET_COMMAND + baseurl + file;
    else
        getfile += "cat " + file;

    QString md5sumcmd = "";
    if (md5sum != "")
        md5sumcmd = "tee " MD5SUM_NAMEDPIPE " | ";

    /* Use pipe viewer for actual processing speed */
    QString cmd = QString("%1 | %2" PIPEVIEWER_COMMAND " | cat > %3")
        .arg(getfile, md5sumcmd, TEMP_MOUNT_FOLDER "/" + file);

    qDebug() << "Copying file" << file;
    return runwritecmd(cmd, md5sum != "");
}

bool MultiImageWriteThread::untar(const QString &baseurl, const QString &tarball)
{
    QString getfile;
    if (!baseurl.isEmpty())
        getfile = WGET_COMMAND + baseurl + tarball;
    else
        getfile = "cat " + tarball;

    QString uncompresscmd = getUncompressCommand(tarball, false);

    QString cmd = QString("%1 | %2 | tar x -C " TEMP_MOUNT_FOLDER)
        .arg(getfile, uncompresscmd);

    qDebug() << "Uncompress file" << tarball;
    return runwritecmd(cmd, false);
}

bool MultiImageWriteThread::dd(const QString &baseurl, const QString &device, RawFileInfo *rawFile)
{
    QString getfile;
    const QString& file = rawFile->filename();
    if (!baseurl.isEmpty())
        getfile = WGET_COMMAND + baseurl + file;
    else
        getfile = "cat " + file;

    QString uncompresscmd = getUncompressCommand(file, false);

    QString cmd = QString("%1 | %2 | dd of=%3 %4")
            .arg(getfile, uncompresscmd, device, rawFile->ddOptions());

    qDebug() << "Raw dd file" << file;
    return runwritecmd(cmd, false);
}

bool MultiImageWriteThread::nandflash(const QString &baseurl, const QString &device, RawFileInfo *rawFile)
{
    QString getfile;
    const QString& file = rawFile->filename();
    if (!baseurl.isEmpty())
        getfile = WGET_COMMAND + baseurl + file;
    else
        getfile = "cat " + file;

    QString uncompresscmd = getUncompressCommand(file, false);

    /* Use pipe viewer to get bytes processed during the command */
    QString cmd = QString("%1 | %2 | nandwrite --quiet --pad %3 %4 -")
            .arg(getfile, uncompresscmd, rawFile->nandwriteOptions(), device);

    qDebug() << "Raw flash file" << file;
    return runwritecmd(cmd, false);
}

bool MultiImageWriteThread::ubiflash(const QString &baseurl, const QString &device, RawFileInfo *rawFile)
{
    QString getfile;
    qint64 size = 0;
    const QString& file = rawFile->filename();
    if (!baseurl.isEmpty()) {
        QString url = baseurl + file;
        // Get Size of file first...
        QByteArray output;
        QStringList wgetargs;
        wgetargs << "--quiet" << "--server-response" << url;
        if (runCommand("wget", wgetargs, output))
        {
            foreach(QByteArray line, output.split('\n')) {
                int colon = line.indexOf(':');
                QByteArray headerName = line.left(colon).trimmed();
                QByteArray headerValue = line.mid(colon + 1).trimmed();

                if (headerName == "Content-Length") {
                    size = QString(headerValue).toLongLong();
                    break;
                }
            }
        }
        if (!size)
        {
            emit error(tr("Failed to get size of URL %1").arg(url));
            return false;
        }

        getfile = WGET_COMMAND + url;
    } else {
        size = QFile(_image->folder() + QDir::separator() + file).size();
        getfile = "cat " + file;
    }

    QString uncompresscmd = getUncompressCommand(file, false);
    QString cmd = QString("%1 | %2 | ubiupdatevol %3 --size=%4 -")
        .arg(getfile, uncompresscmd, device, QString::number(size));

    qDebug() << "Raw flash file" << file;
    return runwritecmd(cmd, false);
}

QString MultiImageWriteThread::getUncompressCommand(const QString &file, bool md5sum)
{
    QString cmd;

    if (md5sum)
        cmd = "tee " MD5SUM_NAMEDPIPE " | ";

    if (file.endsWith(".gz"))
    {
        cmd += "gzip -dc";
    }
    else if (file.endsWith(".xz"))
    {
        cmd += "xz -dc";
    }
    else if (file.endsWith(".bz2"))
    {
        cmd += "bzcat";
    }
    else if (file.endsWith(".lzo"))
    {
        cmd += "lzop -dc";
    }
    else if (file.endsWith(".zip"))
    {
        /* Note: the image must be the only file inside the .zip */
        cmd += "unzip -p";
    }
    else
    {
        return cmd + PIPEVIEWER_COMMAND;
    }

    /* Send data to md5sum named pipe first, then unpack, then send to pipe viewer pipe (progress) */
    return cmd + " | " PIPEVIEWER_COMMAND;
}

bool MultiImageWriteThread::partclone_restore(const QString &baseurl, const QString &image, const QString &device)
{
    QString getfile;
    if (!baseurl.isEmpty())
        getfile = WGET_COMMAND + baseurl + image;
    else
        getfile = "cat " + image;

    QString uncompresscmd = getUncompressCommand(image, false);

    /* Use pipe viewer to get bytes processed during the command */
    QString cmd = QString("%1 | %2 | partclone.restore -q -s - -o %3")
            .arg(getfile, uncompresscmd, device);

    qDebug() << "Partclone " << image;

    return runwritecmd(cmd, false);
}

QByteArray MultiImageWriteThread::getBlkId(const QString &part, const QString &tag)
{
    QByteArray result;
    QProcess p;
    p.start("/sbin/blkid -s " + tag + " -o value " + part);
    p.waitForFinished();

    if (p.exitCode() == 0)
        result = p.readAll().trimmed();

    return result;
}

QByteArray MultiImageWriteThread::getLabel(const QString &part)
{
    return getBlkId(part, "LABEL");
}

QByteArray MultiImageWriteThread::getUUID(const QString &part)
{
    return getBlkId(part, "UUID");
}

QByteArray MultiImageWriteThread::getFsType(const QString &part)
{
    return getBlkId(part, "TYPE");
}
