#include "multiimagewritethread.h"
#include "dto/blockdevinfo.h"
#include "dto/mtddevcontentinfo.h"
#include "dto/mtddevinfo.h"
#include "dto/mtdpartitioninfo.h"
#include "dto/partitioninfo.h"
#include "dto/rawfileinfo.h"
#include "dto/ubivolumeinfo.h"
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

MultiImageWriteThread::MultiImageWriteThread(QObject *parent) :
    QThread(parent), _extraSpacePerPartition(0), _bytesWritten(0)
{
    QDir dir;

    if (!dir.exists(TEMP_MOUNT_FOLDER))
        dir.mkpath(TEMP_MOUNT_FOLDER);
}

void MultiImageWriteThread::setImage(const QString &folder, const QString &infofile, const QString &baseurl, enum ImageSource source)
{
    _image = new ImageInfo(folder, infofile, baseurl, source, this);
}

void MultiImageWriteThread::setConfigBlock(ConfigBlock *configBlock)
{
    _configBlock = configBlock;
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

    if (mtddevs) {
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

    qDebug() << "Executing: /bin/sh" << cmd;
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

bool MultiImageWriteThread::runCommand(QString cmd, QStringList args, QByteArray &output)
{
    QProcess p;

    qDebug() << "Executing: " << cmd << args;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.setWorkingDirectory(_image->folder());
    p.start(cmd, args);

    p.waitForFinished(30000);

    output = p.readAll();
    qDebug() << "Output:" << output;
    qDebug() << "Finished with exit code:" << p.exitCode();
    return p.exitCode() == 0;
}

bool MultiImageWriteThread::processMtdDev(MtdDevInfo *mtddev)
{
    QByteArray device = mtddev->mtdDevice();

    MtdDevContentInfo *content = mtddev->content();
    if (content != NULL) {
        if (!processMtdContent(content, device))
            return false;
    }

    QList<UbiVolumeInfo *> *volumes = mtddev->ubiVolumes();
    if (volumes->length() > 0) {
        if (!processUbi(volumes, device))
            return false;
    }

    return true;
}

bool MultiImageWriteThread::processBlockDev(BlockDevInfo *blockdev)
{
    /* Make sure block device is writeable */
    disableBlockDevForceRo(blockdev->name());

    QList<PartitionInfo *> *partitions = blockdev->partitions();
    if (!partitions->isEmpty())
    {
        /* Writing partition table to this blockdev */
        if (!processPartitions(blockdev, partitions))
            return false;
    }

    /* Writing raw content ignoring parition layout to this blockdev */
    BlockDevContentInfo *content = blockdev->content();
    if (content != NULL) {
        QByteArray device = blockdev->blockDevice();
        if (!processContent(content, device))
            return false;
    }

    return true;
}

bool MultiImageWriteThread::processPartitions(BlockDevInfo *blockdev, QList<PartitionInfo *> *partitions)
{
    int totalnominalsize = 0, totaluncompressedsize = 0, numparts = 0, numexpandparts = 0;

    /* Calculate space requirements, and check special requirements */
    int totalSectors = getFileContents("/sys/class/block/" + blockdev->name() + "/size").trimmed().toULongLong();

    /* We need PARTITION_ALIGNMENT sectors at the begining for MBR and general alignment (currently 4MiB) */
    int availableMB = (totalSectors - PARTITION_ALIGNMENT) / 2048;

    /* key: partition number, value: partition information */
    QMap<int, PartitionInfo *> partitionMap;

    foreach (PartitionInfo *partition, *partitions)
    {
        qDebug() << "Partition:" << numparts << "Type:" << partition->partitionType();
        numparts++;
        if ( partition->wantMaximised() )
            numexpandparts++;
        totalnominalsize += partition->partitionSizeNominal();
        BlockDevContentInfo *content = partition->content();
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
    foreach (PartitionInfo *partition, *(blockdev->partitions()))
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
    QList<PartitionInfo *> partitionList = partitionMap.values();
    foreach (PartitionInfo *p, partitionList)
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
    foreach (PartitionInfo *p, partitionMap.values())
    {
        if (p->partitionSizeSectors())
            QProcess::execute("/bin/dd count=1 bs=512 if=/dev/zero of="+p->partitionDevice());
    }

    /* Install each partition */
    foreach (PartitionInfo *p, *partitions)
    {
        BlockDevContentInfo *content = p->content();
        if (content != NULL) {
            QByteArray partdevice = p->partitionDevice();
            if (!processContent(content, partdevice))
                return false;
        }
    }

    return true;
}


bool MultiImageWriteThread::writePartitionTable(QByteArray blockdevpath, const QMap<int, PartitionInfo *> &partitionMap)
{
    /* Write partition table using sfdisk */
    QByteArray partitionTable;
    for (int i=1; i <= partitionMap.keys().last(); i++)
    {
        if (partitionMap.contains(i))
        {
            PartitionInfo *p = partitionMap.value(i);

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
    proc.start("/usr/sbin/sfdisk -uS " + blockdevpath);
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
            PartitionInfo *p = partitionMap.value(i);
            while (!QFile::exists(p->partitionDevice()))
                QThread::msleep(10);
        }
    }

    return true;
}

bool MultiImageWriteThread::eraseMtdDevice(QByteArray mtddevice)
{
    QStringList eraseargs;
    QByteArray output;
    eraseargs << "--quiet" << mtddevice << "0" << "0";

    return runCommand("/usr/sbin/flash_erase", eraseargs, output);
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

    // At this point UBI should be attached, check Volumes...
    int ubivolid = 0;
    foreach (UbiVolumeInfo *ubivol, *volumes) {
        QByteArray output;
        QStringList ubimkvolargs;
        ubimkvolargs << "/dev/ubi0" << "-N" << ubivol->name();

        if (ubivol->size() > 0)
            ubimkvolargs << "-s" << QString("%1KiB").arg(ubivol->size());
        else
            ubimkvolargs << "-m";


        runCommand("/usr/sbin/ubimkvol", ubimkvolargs, output);

        QString ubivoldev = QString("/dev/ubi0_%1").arg(ubivolid);

        MtdDevContentInfo *contentInfo = ubivol->content();
        if (contentInfo->fsType() == "raw") {
            /* Typically Kernel/Device Tree */
            QStringList ubiupdatevolargs;
            ubiupdatevolargs << ubivoldev << contentInfo->rawFiles()->first()->filename();
            runCommand("/usr/sbin/ubiupdatevol", ubiupdatevolargs, output);
        } else if(contentInfo->fsType() == "ubifs") {
            /* Typically the rootfs */
            QStringList mkfsargs;
            mkfsargs << ubivoldev;
            runCommand("/usr/sbin/mkfs.ubifs", mkfsargs, output);

            QStringList mountargs;
            mountargs << "-t" << "ubifs" << ubivoldev << TEMP_MOUNT_FOLDER;

            if (!runCommand("mount", mountargs, output))
            {
                emit error(tr("Error mounting file system") + "\n" + output);
                result = false;
                break;
            }

            QString tarball = contentInfo->filename();
            QStringList filelist = contentInfo->filelist();
            result = processFileCopy(tarball, filelist);

            QProcess::execute("umount " TEMP_MOUNT_FOLDER);

            if (!result)
                break;
        }

        ubivolid++;
    }

    runCommand("/usr/sbin/ubidetach", ubiattachargs, output);

    return result;
}

bool MultiImageWriteThread::processMtdContent(MtdDevContentInfo *content, QByteArray mtddevice)
{
    QString os_name = _image->name();
    QByteArray fstype = content->fsType();

    if (fstype == "raw" && content->rawFiles()->length() > 0)
    {
        RawFileInfo *rawFileInfo = content->rawFiles()->first();
        QString filename = rawFileInfo->filename();

        emit statusUpdate(tr("%1: Writing raw file").arg(os_name));
        if (!eraseMtdDevice(mtddevice)) {
            emit error(tr("Erasing flash failed"));
            return false;
        }

        QByteArray output;
        QStringList flashargs;
        flashargs << filename << mtddevice;

        if (!runCommand("/usr/sbin/flashcp", flashargs, output)) {
            emit error(tr("Flashing failed") + "\n" + output);
            return false;
        }
    }
    return true;
}

bool MultiImageWriteThread::processFileCopy(QString tarball, QStringList filelist)
{
    QString os_name = _image->name();
    bool resultfile = true;
    bool resultfilelist = true;

    if (!tarball.isEmpty()) {
        if (tarball.startsWith("http"))
            emit statusUpdate(tr("%1: Downloading and extracting filesystem").arg(os_name));
        else
            emit statusUpdate(tr("%1: Extracting filesystem").arg(os_name));

        resultfile = untar(tarball);
    }

    if (!filelist.isEmpty()) {
        emit statusUpdate(tr("%1: Downloading/Copying files").arg(os_name));
        foreach(QString file,filelist) {
            if (!copy(_image->baseUrl(), file)) {
                resultfilelist = false;
                break;
            }
        }
    }

    return resultfile && resultfilelist;
}

bool MultiImageWriteThread::processContent(BlockDevContentInfo *content, QByteArray partdevice)
{
    QString os_name = _image->name();
    QByteArray fstype   = content->fsType();
    QByteArray mkfsopt  = content->mkfsOptions();
    QByteArray label = content->label();
    QString tarball  = content->filename();
    QStringList filelist  = content->filelist();
    QList<RawFileInfo *> *rawFiles = content->rawFiles();


    if (ImageInfo::isNetwork(_image->imageSource()) && !tarball.isEmpty()) {
        tarball = _image->baseUrl() + tarball;
    }

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
        emit statusUpdate(tr("%1: Writing raw files").arg(os_name));
        QString productNumber = _configBlock->getProductNumber();

        foreach (RawFileInfo *rawFile, *rawFiles) {
            /* Only check against product id if specified */
            if (!rawFile->productIds().isEmpty()) {
                if (!rawFile->productIds().contains(productNumber))
                    continue;
            }

            if (!dd(_image->baseUrl(), partdevice, rawFile))
                return false;
        }
    }
    else if (fstype.startsWith("partclone"))
    {
        emit statusUpdate(tr("%1: Writing partition clone image").arg(os_name));
        if (!partclone_restore(tarball, partdevice))
            return false;
    }
    else if (fstype != "unformatted")
    {
        emit statusUpdate(tr("%1: Creating filesystem (%2)").arg(os_name, QString(fstype)));
        if (!mkfs(partdevice, fstype, label, mkfsopt))
            return false;

        /* If there is no tarball/filelist specified, its an empty partition which is perfectly ok too */
        if (tarball.isEmpty() && filelist.isEmpty())
            return true;

        emit statusUpdate(tr("%1: Mounting file system").arg(os_name));
        QString mountcmd;
        if (fstype == "ntfs")
            mountcmd = "/sbin/mount.ntfs-3g ";
        else
            mountcmd = "mount ";
        if (QProcess::execute(mountcmd + partdevice + " " TEMP_MOUNT_FOLDER) != 0)
        {
            emit error(tr("%1: Error mounting file system").arg(os_name));
            return false;
        }

        bool resultfilecopy = processFileCopy(tarball, filelist);

        QProcess::execute("umount " TEMP_MOUNT_FOLDER);

        return resultfilecopy;
    }

    return true;
}

bool MultiImageWriteThread::mkfs(const QByteArray &device, const QByteArray &fstype, const QByteArray &label, const QByteArray &mkfsopt)
{
    QString cmd;

    if (fstype == "fat" || fstype == "FAT")
    {
        cmd = "/usr/sbin/mkfs.fat ";
        if (!label.isEmpty())
        {
            cmd += "-F 32 -n "+label+" ";
        }
    }
    else if (fstype == "ext3")
    {
        cmd = "/sbin/mkfs.ext3 ";
        if (!label.isEmpty())
        {
            cmd += "-L "+label+" ";
        }
    }
    else if (fstype == "ext4")
    {
        cmd = "/sbin/mkfs.ext4 ";
        if (!label.isEmpty())
        {
            cmd += "-L "+label+" ";
        }
    }
    else if (fstype == "ntfs")
    {
        cmd = "/sbin/mkfs.ntfs --fast ";
        if (!label.isEmpty())
        {
            cmd += "-L "+label+" ";
        }
    }

    if (!mkfsopt.isEmpty())
        cmd += mkfsopt+" ";

    cmd += device;

    qDebug() << "Executing:" << cmd;
    QProcess p;
    p.setProcessChannelMode(p.MergedChannels);
    p.start(cmd);
    p.closeWriteChannel();
    p.waitForFinished(-1);

    if (p.exitCode() != 0)
    {
        emit error(tr("Error creating file system")+"\n"+p.readAll());
        return false;
    }

    return true;
}

bool MultiImageWriteThread::isLabelAvailable(const QByteArray &label)
{
    return (QProcess::execute("/sbin/findfs LABEL="+label) != 0);
}

bool MultiImageWriteThread::runwritecmd(const QString &cmd)
{
    QTime t1;
    t1.start();

    QProcess p;
    if (!ImageInfo::isNetwork(_image->imageSource()))
        p.setWorkingDirectory(_image->folder());
    p.start(cmd);
    p.closeWriteChannel();
    p.setReadChannel(QProcess::StandardError);

    /* Parse pipe viewer output for progress */
    qint64 bytes = 0;
    QByteArray line;
    bool ok;
    while (p.waitForReadyRead(-1))
    {
        line = p.readLine();
        qint64 tmp;

        tmp = line.trimmed().toLongLong(&ok);

        if (ok) {
            bytes = tmp;
            emit imageProgress(_bytesWritten + bytes);
        } else {
            break;
        }
    }

    _bytesWritten += bytes;

    p.waitForFinished(-1);

    if (p.exitCode() != 0)
    {
        QByteArray msg;
        if (!ok)
            msg = line;
        msg += p.readAll();
        emit error(tr("Error downloading or writing image")+"\n" + msg);
        return false;
    }
    qDebug() << "Finished writing" << _bytesWritten << "bytes in" << (t1.elapsed()/1000.0) << "seconds";

    return true;
}

bool MultiImageWriteThread::copy(const QString &baseurl, const QString &file)
{
    QString cmd = "sh -o pipefail -c \"";
    if (!baseurl.isEmpty())
        cmd += WGET_COMMAND + baseurl + file;
    else
        cmd += "cat " + file;

    /* Use pipe viewer for actual processing speed */
    cmd += " | pv -b -n";
    cmd += " | cat > " TEMP_MOUNT_FOLDER "/" + file;
    cmd += " \"";

    qDebug() << "Copying file" << file;
    return runwritecmd(cmd);
}


bool MultiImageWriteThread::untar(const QString &tarball)
{
    QString cmd = "sh -o pipefail -c \"";

    if (isURL(tarball))
        cmd += WGET_COMMAND + tarball + " | ";

    QString uncompresscmd = getUncompressCommand(tarball);
    if (uncompresscmd == NULL) {
        emit error(tr("Unknown compression format file extension. Expecting .lzo, .gz, .xz, .bz2 or .zip"));
        return false;
    }

    cmd += uncompresscmd;

    if (!isURL(tarball))
    {
        cmd += " " + tarball;
    }

    /* Use pipe viewer for actual processing speed */
    cmd += " | pv -b -n";

    cmd += " | tar x -C " TEMP_MOUNT_FOLDER;
    cmd += " \"";

    qDebug() << "Uncompress file" << tarball;
    return runwritecmd(cmd);
}

bool MultiImageWriteThread::dd(const QString &baseurl, const QString &device, RawFileInfo *rawFile)
{
    QString cmd = "sh -o pipefail -c \"";
    const QString& file = rawFile->filename();
    if (!baseurl.isEmpty())
        cmd += WGET_COMMAND + baseurl + file;
    else
        cmd += "cat " + file;

    /* For dd images compression is optional */
    QString uncompresscmd = getUncompressCommand(file);
    if (uncompresscmd != NULL)
        cmd += " | " + uncompresscmd;

    /* Use pipe viewer for actual processing speed */
    cmd += " | pv -b -n";

    cmd += " | dd of=" + device + " " + rawFile->ddOptions(); // BusyBox can't do this: +" conv=fsync obs=4M\"";

    qDebug() << "Raw dd file" << file;
    return runwritecmd(cmd);
}

QString MultiImageWriteThread::getUncompressCommand(const QString &file)
{
    if (file.endsWith(".gz"))
    {
        return "gzip -dc";
    }
    else if (file.endsWith(".xz"))
    {
        return "xz -dc";
    }
    else if (file.endsWith(".bz2"))
    {
        return "bzcat";
    }
    else if (file.endsWith(".lzo"))
    {
        return "lzop -dc";
    }
    else if (file.endsWith(".zip"))
    {
        /* Note: the image must be the only file inside the .zip */
        return "unzip -p";
    }
    else
    {
        return "cat";
    }
}

bool MultiImageWriteThread::partclone_restore(const QString &imagePath, const QString &device)
{
    QString cmd = "sh -o pipefail -c \"";

    if (isURL(imagePath))
        cmd += WGET_COMMAND + imagePath + " | ";


    if (!isURL(imagePath))
        cmd += " "+imagePath;

    /* Use pipe viewer for actual processing speed */
    cmd += " | pv -b -n";

    cmd += " | partclone.restore -q -s - -o "+device+" \"";

    return runwritecmd(cmd);
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

QString MultiImageWriteThread::getDescription(const QString &folder, const QString &flavour)
{
    if (QFile::exists(folder+"/flavours.json"))
    {
        QVariantMap v = Json::loadFromFile(folder+"/flavours.json").toMap();
        QVariantList fl = v.value("flavours").toList();

        foreach (QVariant f, fl)
        {
            QVariantMap fm  = f.toMap();
            if (fm.value("name").toString() == flavour)
            {
                return fm.value("description").toString();
            }
        }
    }
    else if (QFile::exists(folder+"/os.json"))
    {
        QVariantMap v = Json::loadFromFile(folder+"/os.json").toMap();
        return v.value("description").toString();
    }

    return "";
}

bool MultiImageWriteThread::isURL(const QString &s)
{
    return s.startsWith("http:") || s.startsWith("https:");
}
