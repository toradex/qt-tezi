#include "usbgadget.h"

#include <QString>
#include <QDebug>
#include <QProcess>

#include "usbgadgethelper.h"
#include <usbg/usbg.h>

UsbGadget::UsbGadget(QObject *parent) : QObject(parent),
    _gadgetInitialized(false), _gadgetIsMassStorage(false)
{
    if (usbgadget_init()) {
        qDebug() << "USB Gadget: Error initalizing:" << usbgadget_strerror();
        return;
    }
    _gadgetInitialized = true;
}

bool UsbGadget::initMassStorage()
{
    if (!_gadgetInitialized)
        return false;

    if (usbgadget_ms_init()) {
        qDebug() << "USB Gadget: Error initalizing Mass Storage:" << usbgadget_strerror();
        return false;
    }

    return true;
}

void UsbGadget::enableMassStorage(bool enable)
{
    if (enable) {
        if (usbgadget_ms_enable(_eMMCDev.toStdString().c_str())) {
            qDebug() << "USB Gadget: Error enabling Mass Storage" << usbgadget_strerror();
            return;
        }
        qDebug() << "USB Gadget: Mass Storage enabled";
        _gadgetIsMassStorage = true;
    } else {
        int ret = usbgadget_ms_disable();
        qDebug() << "USB Gadget: Mass Storage disabled, ret " << ret;
        _gadgetIsMassStorage = false;
    }
}

bool UsbGadget::isMassStorageSafeToRemove()
{
    return usbgadget_ms_safe_to_remove();
}

bool UsbGadget::setMassStorageAttrs(const QString &serial, const QString &productName, const uint16_t idProduct)
{
    if (usbgadget_ms_set_attrs(serial.toStdString().c_str(), productName.toStdString().c_str(), idProduct)) {
        qDebug() << "USB Gadget: Error setting Mass Storage Attributes:" << usbgadget_strerror();
        return false;
    }

    return true;
}

bool UsbGadget::initNcm()
{
    if (!_gadgetInitialized)
        return false;

    if (usbgadget_ncm_init()) {
        qDebug() << "USB Gadget: Error initalizing NCM:" << usbgadget_strerror();
        return false;
    }

    return true;
}

void UsbGadget::enableNcm(bool enable)
{
    if (enable) {
        if (usbgadget_ncm_enable()) {
            qDebug() << "USB Gadget: Error enabling NCM" << usbgadget_strerror();
            return;
        }
        qDebug() << "USB Gadget: NCM enabled";
    } else {
        usbgadget_ncm_disable();
        qDebug() << "USB Gadget: NCM disabled";
    }
}

bool UsbGadget::setNcmAttrs(const QString &serial, const QString &productName, const uint16_t idProduct)
{
    if (usbgadget_ncm_set_attrs(serial.toStdString().c_str(), productName.toStdString().c_str(), idProduct)) {
        qDebug() << "USB Gadget: Error setting NCM Attributes:" << usbgadget_strerror();
        return false;
    }

    return true;
}
