#include <QDebug>
#include <QFile>
#include <QtEndian>
#include <QRegularExpression>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include "configblock.h"
#include "util.h"

const char* const toradex_modules[] = {
	 [0] = "UNKNOWN MODULE",
	 [1] = "0001 Colibri PXA270 312MHz",
	 [2] = "0002 Colibri PXA270 520MHz",
	 [3] = "0003 Colibri PXA320 806MHz",
	 [4] = "0004 Colibri PXA300 208MHz",
	 [5] = "0005 Colibri PXA310 624MHz",
	 [6] = "0006 Colibri PXA320IT 806MHz",
	 [7] = "0007 Colibri PXA300 208MHz XT",
	 [8] = "0008 Colibri PXA270 312MHz",
	 [9] = "0009 Colibri PXA270 520MHz",
	[10] = "0010 Colibri VF50 128MB",
	[11] = "0011 Colibri VF61 256MB",
	[12] = "0012 Colibri VF61 256MB IT",
	[13] = "0013 Colibri VF50 128MB IT",
	[14] = "0014 Colibri iMX6S 256MB",
	[15] = "0015 Colibri iMX6DL 512MB",
	[16] = "0016 Colibri iMX6S 256MB IT",
	[17] = "0017 Colibri iMX6DL 512MB IT",
	[18] = "0018 UNKNOWN MODULE",
	[19] = "0019 UNKNOWN MODULE",
	[20] = "0020 Colibri T20 256MB",
	[21] = "0021 Colibri T20 512MB",
	[22] = "0022 Colibri T20 512MB IT",
	[23] = "0023 Colibri T30 1GB",
	[24] = "0024 Colibri T20 256MB IT",
	[25] = "0025 Apalis T30 2GB",
	[26] = "0026 Apalis T30 1GB",
	[27] = "0027 Apalis iMX6Q 1GB",
	[28] = "0028 Apalis iMX6Q 2GB IT",
	[29] = "0029 Apalis iMX6D 512MB",
	[30] = "0030 Colibri T30 1GB IT",
	[31] = "0031 Apalis T30 1GB IT",
	[32] = "0032 Colibri iMX7S 256MB",
	[33] = "0033 Colibri iMX7D 512MB",
	[34] = "0034 Apalis TK1 2GB",
	[35] = "0035 Apalis iMX6D 1GB IT",
	[36] = "0036 Colibri iMX6ULL 256MB",
	[37] = "0037 Apalis iMX8QM 4GB WB IT",
	[38] = "0038 Colibri iMX8QXP 2GB WB IT",
	[39] = "0039 Colibri iMX7D 1GB",
	[40] = "0040 Colibri iMX6ULL 512MB WB IT",
	[41] = "0041 Colibri iMX7D 512MB EPDC",
	[42] = "0042 Apalis TK1 4GB",
	[43] = "0043 Colibri T20 512MB IT SETEK",
	[44] = "0044 Colibri iMX6ULL 512MB IT",
	[45] = "0045 Colibri iMX6ULL 512MB WB",
	[46] = "0046 Apalis iMX8QXP 2GB WB IT",
	[47] = "0047 Apalis iMX8QM 4GB IT",
	[48] = "0048 Apalis iMX8QP 2GB WB",
	[49] = "0049 Apalis iMX8QP 2GB",
	[50] = "0050 Colibri iMX8QXP 2GB IT",
	[51] = "0051 Colibri iMX8DX 1GB WB",
	[52] = "0052 Colibri iMX8DX 1GB",
	[53] = "0053 Apalis iMX8QXP 2GB ECC IT",
	[54] = "0054 Apalis iMX8DXP 1GB",
	[55] = "0055 Verdin iMX8M Mini Quad 2GB WB IT",
	[56] = "0056 Verdin iMX8M Nano Quad 1GB WB",
	[57] = "0057 Verdin iMX8M Mini DualLite 1GB",
	[58] = "0058 Verdin iMX8M Plus Quad 4GB WB IT",
	[59] = "0059 Verdin iMX8M Mini Quad 2GB IT",
	[60] = "0060 Verdin iMX8M Mini DualLite 1GB WB IT",
	[61] = "0061 Verdin iMX8M Plus Quad 2GB",
	[62] = "0062 Colibri iMX6ULL 1GB IT",
	[63] = "0063 Verdin iMX8M Plus Quad 4GB IT",
	[64] = "0064 Verdin iMX8M Plus Quad 2GB WB IT",
	[65] = "0065 Verdin iMX8M Plus QuadLite 1GB IT",
	[66] = "0066 Verdin iMX8M Plus Quad 8GB WB",
	[67] = "0067 Apalis iMX8QM 8GB WB IT",
	[68] = "0068 Verdin iMX8M Mini Quad 2GB WB IT",
	[69] = "0069 Verdin AM62 Quad 1GB WB IT",
	[70] = "0070 Verdin iMX8M Plus 8GB WB IT",
	[71] = "0071 Verdin AM62 Solo 512MB",
	[72] = "0072 Verdin AM62 Solo 512MB WB IT",
	[73] = "0073 Verdin AM62 Dual 1GB ET",
	[74] = "0074 Verdin AM62 Dual 1GB IT",
	[75] = "0075 Verdin AM62 Dual 1GB WB IT",
	[76] = "0076 Verdin AM62 Quad 2GB WB IT",
};

const char * const toradex_prototype_modules[] = {
	[0] = "2600 Apalis iMX8QXP 2GB ECC WB IT PROTO",
};

QList<quint32> toradex_ouis = {
    0x00142dU,
    0x8c06cbU,
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
    quint32 oui = qFromBigEndian(addr->oui) >> 8;
    quint32 nic = qFromBigEndian(addr->nic) >> 8;
    qint32 oui_index = toradex_ouis.indexOf(oui);

    /*
     * If indexOf() returns -1, default to the size of toradex_ouis so the behavior is the same
     * as in U-Boot cfgblock implementation. This will in practice return an invalid serial#
     * in the next OUI range not yet assigned.
     */
    if (oui_index < 0)
        oui_index = toradex_ouis.size();

    quint32 serial = (oui_index << 24) + nic;
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
    QString assembly_str;

    if (hw->ver_assembly < 26)
        assembly_str = QString(hw->ver_assembly + 'A');
    else
        assembly_str = QString("#%1").arg(hw->ver_assembly);

    QString boardRev = QString("V%1.%2%3")
            .arg(hw->ver_major)
            .arg(hw->ver_minor)
            .arg(assembly_str);
    return boardRev;
}

QString ConfigBlock::getPID8()
{
    ConfigBlockHw *hw = (ConfigBlockHw *)_hw.data();
    QString prodNumber = getProductNumber();
    QString majVer = QString::number(hw->ver_major);
    QString minVer = QString::number(hw->ver_minor);
    QString assyVer = QString::number(hw->ver_assembly).rightJustified(2, '0');
    return prodNumber + majVer + minVer + assyVer;
}

QString ConfigBlock::getProductName()
{
    quint16 productId = getProductId();
    if (productId >= ARRAY_SIZE(toradex_modules)) {
        if (isTdxPrototypeProdid(productId)) {
            return QString::fromLatin1(toradex_prototype_modules[productId]);
        } else {
            return QString("UNKNOWN");
        }
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
    quint32 serial_uint = serial.toUInt();
    quint32 nic = serial_uint & 0xffffff;
    quint32 oui;

    if (isSerialValid(serial_uint))
        oui = toradex_ouis[serial_uint >> 24];
    else
        oui = toradex_ouis[0];

    qDebug() << "User created Config Block, Product ID:" << productid << "Version:" << version << "Serial:" << serial;

    hw.prodid = productid;
    hw.ver_major = asciiver[1] - '0';
    hw.ver_minor = asciiver[3] - '0';

    if (asciiver[4] >= 'A' && asciiver[4] <= 'Z')
        hw.ver_assembly = asciiver[4] - 'A';
    else if (asciiver[4] == '#')
        hw.ver_assembly = asciiver.mid(5,2).toUInt();
    else
        hw.ver_assembly = 99;

    eth.oui = qToBigEndian(oui << 8);
    eth.nic = qToBigEndian(nic << 8);

    return new ConfigBlock(hw, eth);
}

bool ConfigBlock::isProductSupported(const QString &toradexProductNumber, const QStringList &supportedProductIds)
{
    // Only full PID supported now
    assert(toradexProductNumber.length() == 8);

    QStringList supportedProductIdsPid4 = supportedProductIds.filter(QRegularExpression("^[0-9]{4}$"));
    QStringList supportedProductIdsPid8 = supportedProductIds.filter(QRegularExpression("^[0-9]{8}$"));
    QRegularExpression pid8RangeRe("^([0-9]{8})-([0-9]{8}){0,1}$");
    QStringList supportedProductIdsPid8Range = supportedProductIds.filter(pid8RangeRe);

    if (supportedProductIdsPid8.isEmpty() && supportedProductIdsPid8Range.isEmpty())
    {
        // No Pid8 at all, fallback to Pid4 logic
        return supportedProductIdsPid4.contains(toradexProductNumber.left(4));
    }

    bool pid8RangeMatch = false;
    for (QString pid8Range : supportedProductIdsPid8Range)
    {
        QRegularExpressionMatch match = pid8RangeRe.match(pid8Range);

        // Only process pid8 ranges here...
        assert(match.hasMatch());

        QString from = match.captured(1);
        QString to = match.captured(2);

        // This compares characters by their numeric value, hence this does the trick for us
        // See: https://doc.qt.io/qt-5/qstring.html#comparing-strings
        if (from <= toradexProductNumber && to >= toradexProductNumber)
            pid8RangeMatch = true;

        // If "to" is not given, it still must match this product number
        if (from <= toradexProductNumber && to.isEmpty() && toradexProductNumber.left(4) == from.left(4))
            pid8RangeMatch = true;
    }

    bool pid8Match = supportedProductIdsPid8.contains(toradexProductNumber);
    return pid8Match || pid8RangeMatch;
}

bool ConfigBlock::isSerialValid(quint32 serial)
{
    quint8 oui_count = toradex_ouis.size();
    quint32 total_serials = oui_count << 24;

    return (serial < total_serials);
}
