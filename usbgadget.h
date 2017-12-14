#ifndef USBGADGET_H
#define USBGADGET_H

#include <QObject>

class UsbGadget : public QObject
{
    Q_OBJECT
public:
    explicit UsbGadget(QString &serial, QString &productName, int idProduct, const QString &eMMCDev, QObject *parent = 0);
    bool initMassStorage();
    void enableMassStorage(bool enable);
    bool isMassStorageSafeToRemove();

    bool initRndis();
    void enableRndis(bool enable);

    bool isMassStorage() {
        return _gadgetIsMassStorage;
    }

signals:

public slots:

private:
    bool _gadgetInitialized, _gadgetIsMassStorage;
    QString _eMMCDev;
};

#endif // USBGADGET_H
