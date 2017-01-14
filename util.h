#ifndef UTIL_H
#define UTIL_H

#include <QString>
#include <QByteArray>
#include <QVariant>

QByteArray getFileContents(const QString &filename);
void putFileContents(const QString &filename, const QByteArray &data);
void disableBlockDevForceRo(const QString &blockdev);
QString getUrlPath(const QString& url);
QString getUrlTopDir(const QString& url);
QString getVersionString();
#endif // UTIL_H
