#include <QDebug>
#include <QFile>
#include <QtEndian>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include "configblock.h"
#include "util.h"

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
	[36] = "Colibri iMX6ULL 256MB",
	[37] = "Apalis iMX8 QuadMax 4GB Wi-Fi / BT IT",
	[38] = "Colibri iMX8 QuadXPlus 2GB Wi-Fi / BT IT",
	[39] = "Colibri iMX7 Dual 1GB (eMMC)",
	[40] = "Colibri iMX6ULL 512MB Wi-Fi / BT IT",
	[41] = "Colibri iMX7 Dual 512MB EPDC",
	[42] = "Apalis TK1 4GB",
	[43] = "Colibri T20 512MB IT SETEK",
	[44] = "Colibri iMX6ULL 512MB IT",
	[45] = "Colibri iMX6ULL 512MB Wi-Fi / Bluetooth",
	[46] = "Apalis iMX8 QuadXPlus 2GB Wi-Fi / BT IT",
	[47] = "Apalis iMX8 QuadMax 4GB IT",
	[48] = "Apalis iMX8 QuadPlus 2GB Wi-Fi / BT",
	[49] = "Apalis iMX8 QuadPlus 2GB",
	[50] = "Colibri iMX8 QuadXPlus 2GB IT",
	[51] = "Colibri iMX8 DualX 1GB Wi-Fi / Bluetooth",
	[52] = "Colibri iMX8 DualX 1GB",
	[53] = "Apalis iMX8 QuadXPlus 2GB ECC IT",
	[54] = "Apalis iMX8 DualXPlus 1GB",
};

const char * const toradex_prototype_modules[] = {
	[0] = "Apalis iMX8QXP 2GB ECC Wi / BT IT PROTO",
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

ConfigBlock::ConfigBlock(const ConfigBlockHw &hwsrc, const ConfigBlockEthAddr &ethsrc, QObject *parent) : QObject(parent),
  needsWrite(true)
{
    struct ConfigBlockTag *tag;
    qint32 cboffset = 0;
    _cb.fill('\0', 32);

    // Create a binary config block
    tag = (struct ConfigBlockTag *)(_cb.data() + cboffset);
    tag->id = TAG_VALID;
    tag->flags = TAG_FLAG_VALID;
    tag->len = 0;
    cboffset += 4;

    tag = (struct ConfigBlockTag *)(_cb.data() + cboffset);
    tag->id = TAG_HW;
    tag->flags = TAG_FLAG_VALID;
    tag->len = 2;
    cboffset += 4;

    struct ConfigBlockHw *hw = (struct ConfigBlockHw *)(_cb.data() + cboffset);
    *hw = hwsrc;
    _hw = QByteArray(_cb.data() + cboffset, tag->len * 4);
    cboffset += tag->len * 4;

    tag = (struct ConfigBlockTag *)(_cb.data() + cboffset);
    tag->id = TAG_MAC;
    tag->flags = TAG_FLAG_VALID;
    tag->len = 2;
    cboffset += 4;

    struct ConfigBlockEthAddr *eth = (struct ConfigBlockEthAddr *)(_cb.data() + cboffset);
    *eth = ethsrc;
    _mac = QByteArray(_cb.data() + cboffset, tag->len * 4);
    cboffset += tag->len * 4;

    qDebug() << "Raw Config Block:" << _cb.toHex();
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

bool ConfigBlock::isTdxPrototypeProdid(quint16 prodid)
{
       int prototype_range_max = PROTOTYPE_RANGE_MIN + ARRAY_SIZE(toradex_prototype_modules);

       return ((prodid >= PROTOTYPE_RANGE_MIN) && (prodid < prototype_range_max));
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
    if (productId >= ARRAY_SIZE(toradex_modules)) {
        if (isTdxPrototypeProdid(productId))
            return QString::fromLatin1(toradex_prototype_modules[productId]);
    } else {
        return QString("UNKNOWN");
    }
    return QString::fromLatin1(toradex_modules[productId]);
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

// Note: This currently assumes the config block location is already erased!!
// We need boot block awareness to do this more clever...
void ConfigBlock::writeToMtddev(QString device, qint64 offset)
{
    int writesize = getFileContents("/sys/class/mtd/" + device + "/writesize").trimmed().toInt();

    QFile blockDev("/dev/" + device);
    blockDev.open(QFile::ReadWrite);
    blockDev.seek(calculateAbsoluteOffset(blockDev.handle(), offset));
    int written = blockDev.write(_cb);
    // Make sure we write writesize aligned (fill with 0xff)
    blockDev.write(QByteArray(writesize - (written % writesize), 0xff));
    blockDev.close();
}

void ConfigBlock::writeToBlockdev(QString device, qint64 offset)
{
    QFile blockDev("/dev/" + device);
    blockDev.open(QFile::ReadWrite);
    blockDev.seek(calculateAbsoluteOffset(blockDev.handle(), offset));

    blockDev.write(_cb);
    blockDev.flush();
    fsync(blockDev.handle());
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

ConfigBlock *ConfigBlock::configBlockFromUserInput(quint16 productid, const QString &version, const QString &serial)
{
    QByteArray asciiver = version.toLatin1();
    ConfigBlockHw hw;
    ConfigBlockEthAddr eth;

    qDebug() << "User created Config Block, Product ID:" << productid << "Version:" << version << "Serial:" << serial;

    hw.prodid = productid;
    hw.ver_major = asciiver[1] - '0';
    hw.ver_minor = asciiver[3] - '0';
    hw.ver_assembly = asciiver[4] - 'A';

    eth.oui = qToBigEndian(0x00142dU << 8);
    eth.nic = qToBigEndian(serial.toInt() << 8);

    return new ConfigBlock(hw, eth);
}
