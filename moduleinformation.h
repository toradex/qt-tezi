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
                               enum StorageClass storageClass, QObject *parent = 0);

public:
    static ModuleInformation *detectModule();
    ConfigBlock *readConfigBlock();
    quint64 getStorageSize();

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

    ConfigBlock *_configBlock;


};

#endif // MODULEINFORMATION_H
