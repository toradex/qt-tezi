#ifndef CONFIGBLOCK_H
#define CONFIGBLOCK_H

#include <QObject>
#include <QByteArray>
#include <QStringList>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

#define TAG_VALID	0xcf01
#define TAG_MAC		0x0000
#define TAG_HW		0x0008
#define TAG_INVALID	0xffff

#define TAG_FLAG_VALID	0x1

struct toradex_som {
    quint16 prodid;
    const char *name;
};

extern const struct toradex_som toradex_modules[];

struct ConfigBlockTag {
    quint32 len:14;
    quint32 flags:2;
    quint32 id:16;
} __attribute__((__packed__));

struct ConfigBlockEthAddr {
    quint32 oui:24;
    quint32 nic:24;
} __attribute__((__packed__));


struct ConfigBlockHw {
    quint16 ver_major;
    quint16 ver_minor;
    quint16 ver_assembly;
    quint16 prodid;
} __attribute__((__packed__));

class ConfigBlock : public QObject
{
    Q_OBJECT
public:
    explicit ConfigBlock(const QByteArray &cb, QObject *parent = 0);
    explicit ConfigBlock(const ConfigBlockHw &hwsrc, const ConfigBlockEthAddr &ethsrc, QObject *parent = 0);
    static ConfigBlock *readConfigBlockFromBlockdev(const QString &dev, qint64 offset);
    static ConfigBlock *readConfigBlockFromMtd(const QString &dev, qint64 offset);
    static ConfigBlock *configBlockFromUserInput(quint16 productid, const QString &version, const QString &serial);
    static qint64 calculateAbsoluteOffset(int blockdevHandle, qint64 offset);
    void writeToBlockdev(QString device, qint64 offset);
    void writeToMtddev(QString device, qint64 offset);
    QString getSerialNumber();
    quint16 getProductId();
    QString getProductNumber();
    QString getBoardRev();
    QString getProductName();
    int getToradexModuleIndex(quint16 prodid);
    QString getPID8();

    bool needsWrite;

    static bool isProductSupported(const QString &toradexProductNumber, const QStringList &supportedProductIds);
    static bool isSerialValid(quint32 serial);

private:
    QByteArray _cb;
    QByteArray _mac, _hw;
signals:

public slots:

};

#endif // CONFIGBLOCK_H
