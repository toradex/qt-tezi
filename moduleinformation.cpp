#include "dto/blockdevinfo.h"
#include "moduleinformation.h"
#include "util.h"

#include <QFile>
#include <QDebug>

ModuleInformation::ModuleInformation(QString socId, QList<quint16> productIds,
                                     enum StorageClass storageClass, bool rebootWorks,
                                     bool moduleSupported, QObject *parent) : QObject(parent),
  _socId(socId), _productIds(productIds), _storageClass(storageClass), _rebootWorks(rebootWorks), _moduleSupported(moduleSupported)
{
    switch (_storageClass) {
    case StorageClass::Block:
        _configBlockPartition = BlockDevInfo::getDeviceNameFromSymlink("emmc-boot0");
        _configBlockOffset = Q_INT64_C(-512);
        _erasePartitions << BlockDevInfo::getDeviceNameFromSymlink("emmc")
                         << BlockDevInfo::getDeviceNameFromSymlink("emmc-boot0")
                         << BlockDevInfo::getDeviceNameFromSymlink("emmc-boot1");
        _mainPartition = BlockDevInfo::getDeviceNameFromSymlink("emmc");
        _fwEnvConfig = "/etc/fw_env.config";
        break;
    case StorageClass::Mtd:
        _configBlockPartition = "mtd1";
        _configBlockOffset = Q_INT64_C(0x800);
        _erasePartitions << "mtd2" << "mtd3" <<  "mtd4" <<  "mtd5";
        _mainPartition = "mtd0";
        _fwEnvConfig = "/etc/fw_env_mtd4.config";
        break;
    default:
        qDebug() << "Unknown Storage class";
    }
}

void ModuleInformation::unlockFlash()
{
    switch (_storageClass) {
    case StorageClass::Block:
        /* Disable RO on boot partitions, we are a flashing utility... */
        disableBlockDevForceRo(BlockDevInfo::getDeviceNameFromSymlink("emmc-boot0"));
        disableBlockDevForceRo(BlockDevInfo::getDeviceNameFromSymlink("emmc-boot1"));
        break;
    case StorageClass::Mtd:
        /* No unlock needed */
        break;
    }
}

quint64 ModuleInformation::getStorageSize()
{
    QString sysfsSize = QString("/sys/class/%1/%2/size")
            .arg(getStorageClassString(), _mainPartition);
    quint64 size = getFileContents(sysfsSize).trimmed().toULongLong();
    if (_storageClass == StorageClass::Block)
        return size * 512;

    return size;
}

ConfigBlock *ModuleInformation::readConfigBlock()
{
    ConfigBlock *configBlock = NULL;

    switch (_storageClass) {
    case StorageClass::Mtd:
        configBlock = ConfigBlock::readConfigBlockFromMtd(_configBlockPartition, _configBlockOffset);

        if (!configBlock)
            qDebug() << "Config Block not found on raw NAND";
        break;
    case StorageClass::Block:
        configBlock = ConfigBlock::readConfigBlockFromBlockdev(_configBlockPartition, _configBlockOffset);

        if (configBlock == NULL) {
            qDebug() << "Config Block not found at standard location, trying to read Config Block from alternative locations";
            configBlock = ConfigBlock::readConfigBlockFromBlockdev(_mainPartition, Q_INT64_C(0x500 * 512));
            if (configBlock) {
                qDebug() << "Config Block found, migration will be executed upon flashing a new image...";
                configBlock->needsWrite = true;
            }
        }
        break;
    }

    return configBlock;
}

void ModuleInformation::writeConfigBlockIfNeeded(ConfigBlock *configBlock)
{
    if (!configBlock->needsWrite)
        return;

    switch (_storageClass) {
    case ModuleInformation::StorageClass::Block:
        configBlock->writeToBlockdev(_configBlockPartition, _configBlockOffset);
        break;
    case ModuleInformation::StorageClass::Mtd:
        configBlock->writeToMtddev(_configBlockPartition, _configBlockOffset);
        break;
    }

    qDebug() << "Config Block written to" << _configBlockPartition;
    configBlock->needsWrite = false;
}

const QString ModuleInformation::getHostname()
{
    QFile file("/etc/hostname");
    if (!file.exists()) {
        return "unknown";
    }
    file.open(QFile::ReadOnly);
    QString hostname = file.readLine().trimmed();

    return hostname;
}

ModuleInformation *ModuleInformation::detectModule(QObject *parent)
{
    QString socid;
    QList<quint16> productIds;
    enum StorageClass storageClass;
    bool rebootWorks;
    bool moduleSupported = true;

#if __x86_64__
    socid = "i.MX6Q";
    productIds << 27 << 28 << 29 << 35;
    storageClass = StorageClass::Block;
    rebootWorks = false;
    return new ModuleInformation(socid, productIds, storageClass, rebootWorks, moduleSupported, parent);
#else

    // Try to detect which module we are running on...
    QFile file("/sys/bus/soc/devices/soc0/soc_id");
    if (file.exists()) {
        file.open(QFile::ReadOnly);
        socid = file.readLine().trimmed();
        file.close();
        if (socid == "i.MX6Q") {
            // i.MX 6Quad/Dual are only populated on Apalis currently
            productIds << 27 << 28 << 29 << 35 << 82 << 83 << 84 << 85;
            storageClass = StorageClass::Block;
            rebootWorks = false;
        } else if (socid == "i.MX6DL") {
            // i.MX 6DualLite/Solo are only populated on Colibri currently
            productIds << 14 << 15 << 16 << 17 << 77 << 78 << 79 << 80;
            storageClass = StorageClass::Block;
            rebootWorks = false;
        } else if (socid == "i.MX6ULL") {
            // i.MX 6ULL
            QByteArray compatible = getFileContents("/proc/device-tree/compatible");
            if (compatible.contains("colibri-imx6ull-emmc")) {
                storageClass = StorageClass::Block;
                productIds << 62;
            } else {
                productIds << 36 << 40 << 44 << 45;
                storageClass = StorageClass::Mtd;
            }
            rebootWorks = false;
        } else if (socid == "i.MX7D") {
            QByteArray compatible = getFileContents("/proc/device-tree/compatible");
            if (compatible.contains("colibri_imx7d_emmc") || compatible.contains("colibri-imx7d-emmc")) {
                storageClass = StorageClass::Block;
                productIds << 39;
            } else {
                storageClass = StorageClass::Mtd;
                // Dual and Solo are using the same soc_id currently
                productIds << 32 << 33 << 41 << 81;
            }
            rebootWorks = true;

            // Chip Tape-Out version
            // 1.0 chips had a bug in the Boot ROM which required U-Boot
            // to be written in a special format (3/4 of each page only)
            // Tezi does not support those modules.
            QFile file("/sys/bus/soc/devices/soc0/revision");
            if (file.exists()) {
                if (getFileContents("/sys/bus/soc/devices/soc0/revision").trimmed() == "1.0")
                    moduleSupported = false;
            }
        } else if (socid == "i.MX8QM") {
            // i.MX 8QuadMax/QuadPlus
            productIds << 37 << 47 << 48 << 49 << 67 << 91 << 92 << 93 << 94 << 95;
            storageClass = StorageClass::Block;
            rebootWorks = false;
        } else if (socid == "i.MX8QXP") {
            // i.MX 8QuadXPlus/DualX
            productIds << 38 << 50 << 51 << 52;
            storageClass = StorageClass::Block;
            rebootWorks = false;
        } else if (socid == "i.MX8MM") {
            // i.MX 8M Mini
            productIds << 55 << 57 << 59 << 60 << 68 << 86 << 87 << 90 << 216;
            storageClass = StorageClass::Block;
            rebootWorks = true;
        } else if (socid == "i.MX8MN") {
            // i.MX 8M Nano
            productIds << 56;
            storageClass = StorageClass::Block;
            rebootWorks = true;
        } else if (socid == "i.MX8MP") {
            // i.MX 8M Plus
            productIds << 58 << 61 << 63 << 64 << 65 << 66 << 70 << 97 << 206 << 207 << 208 << 209 << 210;
            storageClass = StorageClass::Block;
            rebootWorks = true;
        } else if (socid == "i.MX95") {
            // i.MX 95
            productIds << 89 << 96 << 201 << 202 << 203 << 204 << 205;
            storageClass = StorageClass::Block;
            rebootWorks = true;
        }
    }

    if (productIds.isEmpty()) {
        // TI K3 uses the family file instead
        QFile familyFile("/sys/bus/soc/devices/soc0/family");
        if (familyFile.exists()) {
            familyFile.open(QFile::ReadOnly);
            QString family = familyFile.readLine().trimmed();
            familyFile.close();

            if (family.contains("AM62X")) {
                socid = "AM62";
                productIds << 69 << 71 << 72 << 73 << 74 << 75 << 76;
                storageClass = StorageClass::Block;
                rebootWorks = true;
            } else if (family.contains("AM62PX")) {
                socid = "AM62P";
                productIds << 99;
                storageClass = StorageClass::Block;
                rebootWorks = true;
            } else if (family.contains("J784S4")) {
                socid = "AM69";
                productIds << 88 << 211 << 212 << 213 << 214 << 215;
                storageClass = StorageClass::Block;
                rebootWorks = false;
            }
        }
    }

    return productIds.isEmpty() ? NULL :
           new ModuleInformation(socid, productIds, storageClass, rebootWorks, moduleSupported, parent);
#endif //__x86_64__
}
