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

    bool initRndis();
    void enableRndis(bool enable);
signals:

public slots:

private:
    bool _gadgetInitialized;
};

#endif // USBGADGET_H
