#include "udcpollthread.h"
#include <chrono>
#include <QDebug>
#include <QDir>

/*
 * Polls until a UDC (USB Device Controller) is available in sysfs.
 *
 * This is needed because the UDC driver may take some time to load
 * and when initializing, libusbgx will assume that the default UDC
 * is already present. Delaying all relevant initialization until
 * a UDC is found avoids complexity.
 */
UdcPollThread::UdcPollThread(QObject *parent) : QThread(parent)
{
}

void UdcPollThread::run() {
    QDir dir ("/sys/class/udc", "*", QDir::Name, QDir::Dirs | QDir::NoDotAndDotDot);

    auto start = std::chrono::steady_clock::now();

    qDebug() << "Scanning for UDC...";

    while (dir.count() == 0) {
        msleep(50);
        dir.refresh();
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    qDebug() << "Waited " << elapsed_ms << " ms for the first UDC entry.";

    emit finished();
}
