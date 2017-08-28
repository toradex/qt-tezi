#include "config.h"
#include "util.h"
#include <sys/ioctl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>
#include <QList>

/*
 * Convenience functions
 *
 * Initial author: Floris Bos
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

void disableBlockDevForceRo(const QString &blockdev)
{
    QFile f("/sys/block/" + blockdev + "/force_ro");
    f.open(f.WriteOnly);
    f.write("0", 1);
    f.close();
}

bool makeFifo(const QString &file)
{
    return mkfifo(file.toLocal8Bit().constData(), S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH) == 0;
}

QString getUrlPath(const QString& url)
{
    int slash = url.lastIndexOf('/');
    return url.left(slash + 1);
}

QString getUrlTopDir(const QString& url)
{
    // Process URL, get top dir from a URL such as
    // http://server/Folder//image.json
    int pos = url.lastIndexOf('/');
    while (url[pos] == '/')
        pos--;
    QString dir = url.left(pos + 1);
    int slash = dir.lastIndexOf('/');
    return dir.right(pos - slash);
}

QString getVersionString()
{
    return QString(QObject::tr("Toradex Easy Installer %1 (g%2) - Built: %3"))
            .arg(VERSION_NUMBER, GIT_VERSION, QString::fromLocal8Bit(__DATE__));
}
