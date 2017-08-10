#include "moduleinformation.h"
#include "util.h"

#include <QFile>
#include <QDebug>

ModuleInformation::ModuleInformation(QString socId, QList<quint16> productIds,
                                     enum StorageClass storageClass, bool rebootWorks,
                                     QObject *parent) : QObject(parent),
  _socId(socId), _productIds(productIds), _storageClass(storageClass), _rebootWorks(rebootWorks)
{
    switch (_storageClass) {
    case StorageClass::Block:
        _configBlockPartition = "mmcblk0boot0";
        _configBlockOffset = Q_INT64_C(-512);
        _erasePartitions << "mmcblk0" << "mmcblk0boot0" << "mmcblk0boot1";
        _mainPartition = "mmcblk0";
        break;
    case StorageClass::Mtd:
        _configBlockPartition = "mtd1";
        _configBlockOffset = Q_INT64_C(0x800);
        _erasePartitions << "mtd2" << "mtd3" <<  "mtd4" <<  "mtd5";
        _mainPartition = "mtd0";
        break;
    default:
        qDebug() << "Unknown Storage class";
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
    bool rebootWorks;
    if (socid == "i.MX7D") {
        // Dual and Solo are using the same soc_id currently
        productIds << 32 << 33;
        storageClass = StorageClass::Mtd;
        rebootWorks = true;
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
    } else {
        return NULL;
    }

    return new ModuleInformation(socid, productIds, storageClass, rebootWorks, parent);
}
