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

const char* const toradex_modules[] = {
     [0] = "UNKNOWN MODULE",
     [1] = "Colibri PXA270 312MHz",
     [2] = "Colibri PXA270 520MHz",
     [3] = "Colibri PXA320 806MHz",
     [4] = "Colibri PXA300 208MHz",
     [5] = "Colibri PXA310 624MHz",
     [6] = "Colibri PXA320 806MHz IT",
     [7] = "Colibri PXA300 208MHz XT",
     [8] = "Colibri PXA270 312MHz",
     [9] = "Colibri PXA270 520MHz",
    [10] = "Colibri VF50 128MB", /* not currently on sale */
    [11] = "Colibri VF61 256MB",
    [12] = "Colibri VF61 256MB IT",
    [13] = "Colibri VF50 128MB IT",
    [14] = "Colibri iMX6 Solo 256MB",
    [15] = "Colibri iMX6 DualLite 512MB",
    [16] = "Colibri iMX6 Solo 256MB IT",
    [17] = "Colibri iMX6 DualLite 512MB IT",
    [18] = "UNKNOWN MODULE",
    [19] = "UNKNOWN MODULE",
    [20] = "Colibri T20 256MB",
    [21] = "Colibri T20 512MB",
    [22] = "Colibri T20 512MB IT",
    [23] = "Colibri T30 1GB",
    [24] = "Colibri T20 256MB IT",
    [25] = "Apalis T30 2GB",
    [26] = "Apalis T30 1GB",
    [27] = "Apalis iMX6 Quad 1GB",
    [28] = "Apalis iMX6 Quad 2GB IT",
    [29] = "Apalis iMX6 Dual 512MB",
    [30] = "Colibri T30 1GB IT",
    [31] = "Apalis T30 1GB IT",
    [32] = "Colibri iMX7 Solo 256MB",
    [33] = "Colibri iMX7 Dual 512MB",
    [34] = "Apalis TK1 2GB",
    [35] = "Apalis iMX6 Dual 1GB IT",
};

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
