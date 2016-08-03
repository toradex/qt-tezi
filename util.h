#ifndef UTIL_H
#define UTIL_H

#include <QString>
#include <QByteArray>
#include <QVariant>

/*
 * Convenience functions
 *
 * Initial author: Floris Bos
 * Maintained by Raspberry Pi
 *
 * See LICENSE.txt for license details
 */

extern const char* const toradex_modules[];

QByteArray getFileContents(const QString &filename);
void putFileContents(const QString &filename, const QByteArray &data);
void getOverscan(int &top, int &bottom, int &left, int &right);
QString getUrlPath(const QString& url);
bool canBootOs(const QString& name, const QVariantMap& values);
bool setRebootPartition(QByteArray partition);
#endif // UTIL_H
