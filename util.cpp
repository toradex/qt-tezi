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
