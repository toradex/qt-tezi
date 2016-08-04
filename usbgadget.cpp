#include "usbgadget.h"

#include <QString>
#include <QDebug>

extern "C" {
#include "usbgadgethelper.h"

#include <usbg/usbg.h>
}

UsbGadget::UsbGadget(QObject *parent) : QObject(parent), _gadgetInitialized(false)
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
        if (usbgadget_ms_enable()) {
            qDebug() << "USB Gadget:Error enabling Mass Storage" << usbgadget_strerror();
            return;
        }
        qDebug() << "USB Gadget: Mass Storage enabled";
    } else {
        usbgadget_ms_disable();
        qDebug() << "USB Gadget: Mass Storage disabled";
    }
}

bool UsbGadget::isMassStorageSafeToRemove()
{
    return usbgadget_ms_safe_to_remove();
}
