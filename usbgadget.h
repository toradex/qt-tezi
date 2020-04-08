#ifndef USBGADGET_H
#define USBGADGET_H

#include <QObject>

class UsbGadget : public QObject
{
    Q_OBJECT
public:
    explicit UsbGadget(QObject *parent = 0);
    bool initMassStorage();
    void enableMassStorage(bool enable);
    bool isMassStorageSafeToRemove();
    bool setMassStorageAttrs(const QString &serial, const QString &productName, const uint16_t idProduct);

    bool initRndis();
    void enableRndis(bool enable);
    bool setRndisAttrs(const QString &serial, const QString &productName, const uint16_t idProduct);

    bool isMassStorage() {
        return _gadgetIsMassStorage;
    }

    void setMassStorageDev(const QString &eMMCDev) {
        _eMMCDev = eMMCDev;
    }

signals:

public slots:

private:
    bool _gadgetInitialized, _gadgetIsMassStorage;
    QString _eMMCDev;
};

#endif // USBGADGET_H
