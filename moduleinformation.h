#ifndef MODULEINFORMATION_H
#define MODULEINFORMATION_H

#include <QString>
#include <QList>
#include "configblock.h"

class ModuleInformation : public QObject
{
public:
    enum StorageClass { Block, Mtd };

    Q_OBJECT
protected:
    explicit ModuleInformation(QString socId, QList<quint16> productIds,
                               enum StorageClass storageClass, bool rebootWorks,
                               bool moduleSupported, QObject *parent = 0);

public:
    static const QString getHostname();
    static ModuleInformation *detectModule(QObject *parent);
    ConfigBlock *readConfigBlock();
    void writeConfigBlockIfNeeded(ConfigBlock *configBlock);
    quint64 getStorageSize();
    void unlockFlash();

    inline QList<quint16> &productIds() {
        return _productIds;
    }

    inline enum StorageClass storageClass() {
        return _storageClass;
    }

    inline QString configBlockPartition() {
        return _configBlockPartition;
    }

    inline qint64 configBlockOffset() {
        return _configBlockOffset;
    }

    inline QList<QString> &erasePartitions() {
        return _erasePartitions;
    }

    inline QString &mainPartition() {
        return _mainPartition;
    }

    inline QString &fwEnvConfig() {
        return _fwEnvConfig;
    }

    inline const QString getStorageClassString() {
        switch (_storageClass) {
        case StorageClass::Mtd:
            return QString("mtd");
            break;
        case StorageClass::Block:
            return QString("block");
            break;
        }
        return QString("");
    }

    /*
     * Return true if the module can get stuck in boot ROM when rebooting
     * We can later extend that to runtime detect whether we booted with
     * recovery mode to make that smarter
     */
    inline bool rebootWorks()
    {
        return _rebootWorks;
    }

    inline bool moduleSupported()
    {
        return _moduleSupported;
    }

signals:

public slots:

private:
    QString _socId;
    QList<quint16> _productIds;
    enum StorageClass _storageClass;
    QString _configBlockPartition;
    qint64 _configBlockOffset;
    QList<QString> _erasePartitions;
    QString _mainPartition;
    QString _fwEnvConfig;

    ConfigBlock *_configBlock;
    bool _rebootWorks;
    bool _moduleSupported;
};

#endif // MODULEINFORMATION_H
