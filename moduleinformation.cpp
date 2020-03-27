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
        _configBlockPartition = "mmcblk0boot0";
        _configBlockOffset = Q_INT64_C(-512);
        _erasePartitions << "mmcblk0" << "mmcblk0boot0" << "mmcblk0boot1";
        _mainPartition = "mmcblk0";
        _fwEnvConfig = "/etc/fw_env_mmcblk0boot0.config";
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
        disableBlockDevForceRo("mmcblk0boot0");
        disableBlockDevForceRo("mmcblk0boot1");
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
            configBlock = ConfigBlock::readConfigBlockFromBlockdev(QString("mmcblk0"), Q_INT64_C(0x500 * 512));
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
    // Try to detect which module we are running on...
    QFile file("/sys/bus/soc/devices/soc0/soc_id");
    if (!file.exists()) {
        return NULL;
    }
    file.open(QFile::ReadOnly);
    QString socid = file.readLine().trimmed();
    file.close();
    QList<quint16> productIds;
    enum StorageClass storageClass;
    bool rebootWorks, moduleSupported = true;
    if (socid == "48") {
        // Tegra 3
        QByteArray compatible = getFileContents("/proc/device-tree/compatible");
        if (compatible.contains("apalis")) {
            productIds << 25 << 26 << 31;
        } else {
            productIds << 23 << 30;
        }
        socid = "T30";
        storageClass = StorageClass::Block;
        rebootWorks = true;
    } else if (socid == "64") {
        // Tegra K1
        socid = "TK1";
        productIds << 34 << 42;
        storageClass = StorageClass::Block;
        rebootWorks = true;
    } else if (socid == "i.MX7D") {
        QByteArray compatible = getFileContents("/proc/device-tree/compatible");
        if (compatible.contains("colibri_imx7d_emmc") || compatible.contains("colibri-imx7d-emmc")) {
            storageClass = StorageClass::Block;
            productIds << 39;
        } else {
            storageClass = StorageClass::Mtd;
            // Dual and Solo are using the same soc_id currently
            productIds << 32 << 33 << 41;
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
    } else if (socid == "i.MX6Q") {
        // i.MX6 Quad/Dual are only populated on Apalis currently
        productIds << 27 << 28 << 29 << 35;
        storageClass = StorageClass::Block;
        rebootWorks = false;
    } else if (socid == "i.MX6DL") {
        // i.MX6 DualLite/Solo are only populated on Colibri currently
        productIds << 14 << 15 << 16 << 17;
        storageClass = StorageClass::Block;
        rebootWorks = false;
    } else if (socid == "i.MX6ULL") {
        // i.MX6 ULL
        productIds << 36 << 40 << 44 << 45;
        storageClass = StorageClass::Mtd;
        rebootWorks = false;
    } else if (socid == "i.MX8QM") {
        // i.MX8QM
        productIds << 37 << 47 << 48 << 49;
        storageClass = StorageClass::Block;
        rebootWorks = false;
    } else if (socid == "i.MX8QXP") {
        // i.MX8QXP
        QByteArray compatible = getFileContents("/proc/device-tree/compatible");
        if (compatible.contains("apalis")) {
            productIds << 46 << 53 << 54 << 2600;
        } else {
            productIds << 38 << 50 << 51 << 52;
        }
        storageClass = StorageClass::Block;
        rebootWorks = false;
    } else if (socid == "i.MX8MM") {
        // i.MX8MM
        productIds << 55;
        storageClass = StorageClass::Block;
        rebootWorks = true;
    } else {
        // Downstream the tegras use the machine file instead
        QFile machineFile("/sys/bus/soc/devices/soc0/machine");
        if (!machineFile.exists()) {
            return NULL;
        }
        machineFile.open(QFile::ReadOnly);
        QString machine = machineFile.readLine().trimmed();
        machineFile.close();

        if (machine == "apalis-tk1") {
            socid = "TK1";
            productIds << 34;
            storageClass = StorageClass::Block;
            rebootWorks = true;
        } else {
            return NULL;
        }
    }

    return new ModuleInformation(socid, productIds, storageClass, rebootWorks, moduleSupported, parent);
}
