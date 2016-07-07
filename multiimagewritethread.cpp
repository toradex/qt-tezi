#include "multiimagewritethread.h"
#include "blockdevinfo.h"
#include "config.h"
#include "json.h"
#include "util.h"
#include "osinfo.h"
#include "partitioninfo.h"
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
    QThread(parent), _extraSpacePerPartition(0), _part(5)
{
    QDir dir;

    if (!dir.exists("/mnt2"))
        dir.mkdir("/mnt2");
}

void MultiImageWriteThread::setImage(const QString &folder, const QString &infofile)
{
    _image = new OsInfo(folder, infofile, this);
}

void MultiImageWriteThread::run()
{
    QList<BlockDevInfo *> *blockdevs = _image->blockdevs();
    qDebug() << "Processing OS:" << _image->name();

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

    emit completed();
}

bool MultiImageWriteThread::processBlockDev(BlockDevInfo *blockdev)
{
    /* Make sure block device is writeable */
    QFile f("/sys/block/" + blockdev->name() + "/force_ro");
    f.open(f.WriteOnly);
    f.write("0", 1);
    f.close();

    QList<PartitionInfo *> *partitions = blockdev->partitions();
    if (!partitions->isEmpty())
    {
        /* Writing partition table to this blockdev */
        if (!processPartitions(blockdev, partitions))
            return false;
    } else {
        /* Writing content without a parition table to this blockdev */
        FileSystemInfo *fs = blockdev->content();
        QByteArray device = blockdev->blockDevice();
        if (!processContent(fs, device))
            return false;
    }

    emit statusUpdate(tr("Finish writing (syncing)"));
    sync();

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
        qDebug() << numparts << partition->partitionType();
        numparts++;
        if ( partition->wantMaximised() )
            numexpandparts++;
        totalnominalsize += partition->partitionSizeNominal();
        totaluncompressedsize += partition->uncompressedTarballSize();

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
#ifdef SHRINK_PARTITIONS_TO_MINIMIZE_GAPS
        if (partition->wantMaximised() || (partition->partitionSizeNominal()*2048) % PARTITION_ALIGNMENT != 0)
            totalnominalsize += PARTITION_ALIGNMENT/2048;
#else
        totalnominalsize += PARTITION_ALIGNMENT/2048;
#endif
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

    qDebug() << partitionMap;
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
        FileSystemInfo *fs = p->content();
        QByteArray partdevice = p->partitionDevice();
        if (!processContent(fs, partdevice))
            return false;
    }

    return true;
}


bool MultiImageWriteThread::writePartitionTable(QByteArray blockdevpath, const QMap<int, PartitionInfo *> &partitionMap)
{
    /* Write partition table using sfdisk */
    qDebug() << "Partition map:" << partitionMap;

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

    qDebug() << "sfdisk done, output:" << proc.readAll();
    if (proc.exitCode() != 0)
    {
        emit error(tr("Error creating partition table")+"\n"+proc.readAll());
        return false;
    }

    ::sync();
    QThread::msleep(200);
    proc.start("/usr/sbin/partprobe " + blockdevpath);
    proc.waitForFinished(-1);
    if (proc.exitCode() != 0)
    {
        emit error(tr("Error probing partition table")+"\n"+proc.readAll());
        return false;
    }

    return true;
}

bool MultiImageWriteThread::processContent(FileSystemInfo *fs, QByteArray partdevice)
{
    QString os_name = _image->name();
    QByteArray fstype   = fs->fsType();
    QByteArray mkfsopt  = fs->mkfsOptions();
    QByteArray ddopt    = fs->ddOptions();
    QByteArray label = fs->label();
    QString tarball  = fs->filename();
    bool emptyfs     = fs->emptyFS();

    if (!emptyfs && tarball.isEmpty())
    {
        /* If no tarball URL is specified, we expect the tarball to reside in the folder and be named <label.tar.xz> */
        if (fstype == "raw" || fstype.startsWith("partclone"))
            tarball = _image->folder()+"/"+label+".xz";
        else
            tarball = _image->folder()+"/"+label+".tar.xz";

        if (!QFile::exists(tarball))
        {
            emit error(tr("File '%1' does not exist").arg(tarball));
            return false;
        }
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
        emit statusUpdate(tr("%1: Writing OS image").arg(os_name));
        if (!dd(tarball, partdevice, ddopt))
            return false;
    }
    else if (fstype.startsWith("partclone"))
    {
        emit statusUpdate(tr("%1: Writing OS image").arg(os_name));
        if (!partclone_restore(tarball, partdevice))
            return false;
    }
    else if (fstype != "unformatted")
    {
        emit runningMKFS();
        emit statusUpdate(tr("%1: Creating filesystem (%2)").arg(os_name, QString(fstype)));
        if (!mkfs(partdevice, fstype, label, mkfsopt))
            return false;
        emit finishedMKFS();

        if (!emptyfs)
        {
            emit statusUpdate(tr("%1: Mounting file system").arg(os_name));
            QString mountcmd;
            if (fstype == "ntfs")
                mountcmd = "/sbin/mount.ntfs-3g ";
            else
                mountcmd = "mount ";
            if (QProcess::execute(mountcmd+partdevice+" /mnt2") != 0)
            {
                emit error(tr("%1: Error mounting file system").arg(os_name));
                return false;
            }

            if (tarball.startsWith("http"))
                emit statusUpdate(tr("%1: Downloading and extracting filesystem").arg(os_name));
            else
                emit statusUpdate(tr("%1: Extracting filesystem").arg(os_name));

            bool result = untar(tarball);

            QProcess::execute("umount /mnt2");

            if (!result)
                return false;
        }
    }

    _part++;

/*
    emit statusUpdate(tr("%1: Mounting FAT partition").arg(os_name));
    if (QProcess::execute("mount "+partitions->first()->partitionDevice()+" /mnt2") != 0)
    {
        emit error(tr("%1: Error mounting file system").arg(os_name));
        return false;
    }
  */
    /* Partition setup script can either reside in the image folder
     * or inside the boot partition tarball */
    /*
    QString postInstallScript = image->folder()+"/partition_setup.sh";
    if (!QFile::exists(postInstallScript))
        postInstallScript = "/mnt2/partition_setup.sh";

    if (QFile::exists(postInstallScript))
    {
        emit statusUpdate(tr("%1: Running partition setup script").arg(os_name));
        QProcess proc;
        QProcessEnvironment env;
        QStringList args(postInstallScript);
        env.insert("PATH", "/bin:/usr/bin:/sbin:/usr/sbin");
*/
        /* - Parameters to the partition-setup script are supplied both as
         *   command line parameters and set as environement variables
         * - Boot partition is mounted, working directory is set to its mnt folder
         *
         *  partition_setup.sh part1=/dev/mmcblk0p3 id1=LABEL=BOOT part2=/dev/mmcblk0p4
         *  id2=UUID=550e8400-e29b-41d4-a716-446655440000
         */
    /*
        int pnr = 1;
        foreach (PartitionInfo *p, *partitions)
        {
            QString part  = p->partitionDevice();
            QString nr    = QString::number(pnr);
            QString uuid  = getUUID(part);
            QString label = getLabel(part);
            QString id;
            if (!label.isEmpty())
                id = "LABEL="+label;
            else
                id = "UUID="+uuid;

            qDebug() << "part" << part << uuid << label;

            args << "part"+nr+"="+part << "id"+nr+"="+id;
            env.insert("part"+nr, part);
            env.insert("id"+nr, id);
            pnr++;
        }

        qDebug() << "Executing: sh" << args;
        qDebug() << "Env:" << env.toStringList();
        proc.setProcessChannelMode(proc.MergedChannels);
        proc.setProcessEnvironment(env);
        proc.setWorkingDirectory("/mnt2");
        proc.start("/bin/sh", args);
        proc.waitForFinished(-1);
        qDebug() << proc.exitStatus();

        if (proc.exitCode() != 0)
        {
            emit error(tr("%1: Error executing partition setup script").arg(os_name)+"\n"+proc.readAll());
            return false;
        }
    }

    emit statusUpdate(tr("%1: Unmounting FAT partition").arg(os_name));
    if (QProcess::execute("umount /mnt2") != 0)
    {
        emit error(tr("%1: Error unmounting").arg(os_name));
    }*/

    return true;
}

bool MultiImageWriteThread::mkfs(const QByteArray &device, const QByteArray &fstype, const QByteArray &label, const QByteArray &mkfsopt)
{
    QString cmd;

    if (fstype == "fat" || fstype == "FAT")
    {
        cmd = "/sbin/mkfs.fat ";
        if (!label.isEmpty())
        {
            cmd += "-n "+label+" ";
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

bool MultiImageWriteThread::untar(const QString &tarball)
{
    QString cmd = "sh -o pipefail -c \"";

    if (isURL(tarball))
        cmd += "wget --no-verbose --tries=inf -O- "+tarball+" | ";

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
    cmd += " | tar x -C /mnt2 ";
    cmd += "\"";

    QTime t1;
    t1.start();
    qDebug() << "Executing:" << cmd;

    QProcess p;
    if (!isURL(tarball))
        p.setWorkingDirectory(_image->folder());
    p.setProcessChannelMode(p.MergedChannels);
    p.start(cmd);
    p.closeWriteChannel();
    p.waitForFinished(-1);

    if (p.exitCode() != 0)
    {
        QByteArray msg = p.readAll();
        qDebug() << msg;
        emit error(tr("Error downloading or extracting tarball")+"\n"+msg);
        return false;
    }
    qDebug() << "finished writing filesystem in" << (t1.elapsed()/1000.0) << "seconds";

    return true;
}

bool MultiImageWriteThread::dd(const QString &imagePath, const QString &device, const QByteArray &dd_options)
{
    QString cmd = "sh -o pipefail -c \"";

    if (isURL(imagePath))
        cmd += "wget --no-verbose --tries=inf -O- "+imagePath+" | ";

    /* For dd images compression is optional */
    QString uncompresscmd = getUncompressCommand(imagePath);
    if (uncompresscmd != NULL)
        cmd += uncompresscmd;

    if (!isURL(imagePath))
        cmd += " "+imagePath;

    cmd += " | dd of=" + device + " " + dd_options; // BusyBox can't do this: +" conv=fsync obs=4M\"";

    QTime t1;
    t1.start();
    qDebug() << "Executing:" << cmd;

    QProcess p;
    if (!isURL(imagePath))
        p.setWorkingDirectory(_image->folder());
    p.setProcessChannelMode(p.MergedChannels);
    p.start(cmd);
    p.closeWriteChannel();
    p.waitForFinished(-1);

    if (p.exitCode() != 0)
    {
        emit error(tr("Error downloading or writing image")+"\n"+p.readAll());
        return false;
    }
    qDebug() << "finished writing filesystem in" << (t1.elapsed()/1000.0) << "seconds";

    return true;
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
        cmd += "wget --no-verbose --tries=inf -O- "+imagePath+" | ";


    if (!isURL(imagePath))
        cmd += " "+imagePath;

    cmd += " | partclone.restore -q -s - -o "+device+" \"";

    QTime t1;
    t1.start();
    qDebug() << "Executing:" << cmd;

    QProcess p;
    p.setProcessChannelMode(p.MergedChannels);
    p.start(cmd);
    p.closeWriteChannel();
    p.waitForFinished(-1);

    if (p.exitCode() != 0)
    {
        emit error(tr("Error downloading or writing OS to SD card")+"\n"+p.readAll());
        return false;
    }
    qDebug() << "finished writing filesystem in" << (t1.elapsed()/1000.0) << "seconds";

    return true;
}

void MultiImageWriteThread::patchConfigTxt()
{

        QSettings settings("/settings/noobs.conf", QSettings::IniFormat);
        int videomode = settings.value("display_mode", 0).toInt();

        QByteArray dispOptions;

        switch (videomode)
        {
        case 0: /* HDMI PREFERRED */
            dispOptions = "hdmi_force_hotplug=1\r\n";
            break;
        case 1: /* HDMI VGA */
            dispOptions = "hdmi_ignore_edid=0xa5000080\r\nhdmi_force_hotplug=1\r\nhdmi_group=2\r\nhdmi_mode=4\r\n";
            break;
        case 2: /* PAL */
            dispOptions = "hdmi_ignore_hotplug=1\r\nsdtv_mode=2\r\n";
            break;
        case 3: /* NTSC */
            dispOptions = "hdmi_ignore_hotplug=1\r\nsdtv_mode=0\r\n";
            break;
        }


        QFile f("/mnt2/config.txt");
        f.open(f.Append);
        f.write("\r\n# NOOBS Auto-generated Settings:\r\n"+dispOptions);
        f.close();

}

QByteArray MultiImageWriteThread::getLabel(const QString part)
{
    QByteArray result;
    QProcess p;
    p.start("/sbin/blkid -s LABEL -o value "+part);
    p.waitForFinished();

    if (p.exitCode() == 0)
        result = p.readAll().trimmed();

    return result;
}

QByteArray MultiImageWriteThread::getUUID(const QString part)
{
    QByteArray result;
    QProcess p;
    p.start("/sbin/blkid -s UUID -o value "+part);
    p.waitForFinished();

    if (p.exitCode() == 0)
        result = p.readAll().trimmed();

    return result;
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
