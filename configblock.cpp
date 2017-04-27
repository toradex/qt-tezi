#include <QDebug>
#include <QFile>
#include <QtEndian>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include "configblock.h"
#include "util.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

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

ConfigBlock::ConfigBlock(const QByteArray &cb, QObject *parent) : QObject(parent),
  needsWrite(false), _cb(cb)
{
    qint32 cboffset = 0;

    while (cboffset < cb.size()) {
        struct ConfigBlockTag *tag = (struct ConfigBlockTag *)(cb.data() + cboffset);
        cboffset += 4;

        if (tag->id == TAG_INVALID)
            break;
        if (tag->flags == TAG_FLAG_VALID) {
            switch (tag->id) {
            case TAG_MAC:
                _mac = QByteArray(cb.data() + cboffset, tag->len * 4);
                break;
            case TAG_HW:
                _hw = QByteArray(cb.data() + cboffset, tag->len * 4);
                break;
            }
        }

        cboffset += tag->len * 4;
    }
}

QString ConfigBlock::getSerialNumber()
{
    ConfigBlockEthAddr *addr = (ConfigBlockEthAddr *)_mac.data();
    qint32 serial = qFromBigEndian(addr->nic) >> 8;
    return QString::number(serial).rightJustified(8, '0');
}

quint16 ConfigBlock::getProductId()
{
    ConfigBlockHw *hw = (ConfigBlockHw *)_hw.data();
    return hw->prodid;
}

QString ConfigBlock::getProductNumber()
{
    return QString::number(getProductId()).rightJustified(4, '0');
}

QString ConfigBlock::getBoardRev()
{
    ConfigBlockHw *hw = (ConfigBlockHw *)_hw.data();
    QString boardRev = QString("V%1.%2%3")
            .arg(hw->ver_major)
            .arg(hw->ver_minor)
            .arg(QChar(hw->ver_assembly + 'A'));
    return boardRev;
}

QString ConfigBlock::getProductName()
{
    quint16 productId = getProductId();
    if (productId > ARRAY_SIZE(toradex_modules))
        return QString();
    return QString::fromAscii(toradex_modules[getProductId()]);
}

qint64 ConfigBlock::calculateAbsoluteOffset(int blockdevHandle, qint64 offset)
{
    quint64 size;

    /* Get size using ioctl (QFile's size() does not work for blockdevs) */
    ioctl(blockdevHandle, BLKGETSIZE64, &size);

    /* Negative offset == distance from the end */
    if (offset < 0)
        offset += size;

    return offset;
}

void ConfigBlock::writeToBlockdev(const QString &dev, qint64 offset)
{
    disableBlockDevForceRo(dev);

    QFile blockDev("/dev/" + dev);
    blockDev.open(QFile::ReadWrite);
    blockDev.seek(calculateAbsoluteOffset(blockDev.handle(), offset));

    blockDev.write(_cb);
    blockDev.close();
}

ConfigBlock *ConfigBlock::readConfigBlockFromBlockdev(const QString &dev, qint64 offset)
{
    qDebug() << "Trying to read configblock from" << dev << "at" << offset;

    QFile blockDev("/dev/" + dev);
    blockDev.open(QFile::ReadOnly);
    blockDev.seek(calculateAbsoluteOffset(blockDev.handle(), offset));

    QByteArray cb = blockDev.read(512);
    blockDev.close();
    struct ConfigBlockTag *tag = (struct ConfigBlockTag *)cb.data();

    if (tag->flags != TAG_FLAG_VALID || tag->id != TAG_VALID)
        return NULL;

    return new ConfigBlock(cb);
}

ConfigBlock *ConfigBlock::readConfigBlockFromMtd(const QString &dev, qint64 offset)
{
    qDebug() << "Trying to read configblock from" << dev << "at" << offset;

    QFile mtdDev("/dev/" + dev);
    mtdDev.open(QFile::ReadOnly);
    mtdDev.seek(offset);

    QByteArray cb = mtdDev.read(512);
    mtdDev.close();
    struct ConfigBlockTag *tag = (struct ConfigBlockTag *)cb.data();

    if (tag->flags != TAG_FLAG_VALID || tag->id != TAG_VALID)
        return NULL;

    return new ConfigBlock(cb);
}
