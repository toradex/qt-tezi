#include "usbgadget.h"

#include <QString>
#include <QDebug>
#include <QProcess>

#include "usbgadgethelper.h"
#include <usbg/usbg.h>

UsbGadget::UsbGadget(QString &serial, QString &productName, int idProduct, QObject *parent) : QObject(parent),
    _gadgetInitialized(false), _gadgetIsMassStorage(false)
{
    uint16_t productId = 0x4000 + idProduct;

    if (usbgadget_init(serial.toStdString().c_str(), productName.toStdString().c_str(), productId)) {
        qDebug() << "USB Gadget: Error initalizing:" << usbgadget_strerror();
        return;
    }

    _gadgetInitialized = true;
}

bool UsbGadget::initMassStorage(const QString &emmcdev)
{
    if (!_gadgetInitialized)
        return false;

    if (usbgadget_ms_init(emmcdev.toStdString().c_str())) {
        qDebug() << "USB Gadget: Error initalizing Mass Storage:" << usbgadget_strerror();
        return false;
    }

    return true;
}

void UsbGadget::enableMassStorage(bool enable)
{
    if (enable) {
        if (usbgadget_ms_enable()) {
            qDebug() << "USB Gadget: Error enabling Mass Storage" << usbgadget_strerror();
            return;
        }
        qDebug() << "USB Gadget: Mass Storage enabled";
        _gadgetIsMassStorage = true;
    } else {
        usbgadget_ms_disable();
        qDebug() << "USB Gadget: Mass Storage disabled";
        _gadgetIsMassStorage = false;
    }
}

bool UsbGadget::isMassStorageSafeToRemove()
{
    return usbgadget_ms_safe_to_remove();
}

bool UsbGadget::initRndis()
{
    if (!_gadgetInitialized)
        return false;

    if (usbgadget_rndis_init()) {
        qDebug() << "USB Gadget: Error initalizing RDNIS:" << usbgadget_strerror();
        return false;
    }

    int code = QProcess::execute("/usr/sbin/ifplugd -u 1 -M -f -i usb0 -r /etc/ifplugd/ifplugd.usb.action");
    if (code)
        qDebug() << "RNDIS ifplugd exited with exit code" << code;

    return true;
}

void UsbGadget::enableRndis(bool enable)
{
    if (enable) {
        if (usbgadget_rndis_enable()) {
            qDebug() << "USB Gadget: Error enabling RNDIS" << usbgadget_strerror();
            return;
        }
        qDebug() << "USB Gadget: RNDIS enabled";
    } else {
        usbgadget_rndis_disable();
        qDebug() << "USB Gadget: RNDIS disabled";
    }
}
