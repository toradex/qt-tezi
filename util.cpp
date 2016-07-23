#include "util.h"
#include <sys/ioctl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>
#include <QList>

/*
 * Convenience functions
 *
 * Initial author: Floris Bos
 * Maintained by Raspberry Pi
 *
 * See LICENSE.txt for license details
 */

QByteArray getFileContents(const QString &filename)
{
    QByteArray r;
    QFile f(filename);
    f.open(f.ReadOnly);
    r = f.readAll();
    f.close();

    return r;
}

void putFileContents(const QString &filename, const QByteArray &data)
{
    QFile f(filename);
    f.open(f.WriteOnly);
    f.write(data);
    f.close();
}

QString getUrlPath(const QString& url)
{
    int slash = url.lastIndexOf('/');
    return url.left(slash + 1);
}

/* Whether this OS should be displayed in the list of bootable OSes */
bool canBootOs(const QString& name, const QVariantMap& values)
{
    /* Can't simply pull "name" from "values" because in some JSON files it's "os_name" and in others it's "name" */

    /* Check if it's explicitly not bootable */
    bool bootable = values.value("bootable", true).toBool();
    if (!bootable)
    {
        return false;
    }

    /* Data Partition isn't bootable */
    if (name == "Data Partition")
    {
        return false;
    }

    return true;
}

bool setRebootPartition(QByteArray partition)
{
    if (QFileInfo("/sys/module/bcm2708/parameters/reboot_part").exists())
    {
        putFileContents("/sys/module/bcm2708/parameters/reboot_part", partition+"\n");
        return true;
    }
    else if (QFileInfo("/sys/module/bcm2709/parameters/reboot_part").exists())
    {
        putFileContents("/sys/module/bcm2709/parameters/reboot_part", partition+"\n");
        return true;
    }
    return false;
}
