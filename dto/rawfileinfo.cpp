#include "rawfileinfo.h"

RawFileInfo::RawFileInfo(const QVariantMap &rawfile, QObject *parent) : QObject(parent)
{
    _filename   = rawfile.value("filename").toString();
    _ddOptions  = rawfile.value("dd_options").toString();
    _nandwriteOptions  = rawfile.value("nandwrite_options").toByteArray();
    _size       = rawfile.value("size", 0).toInt();
    _offset = rawfile.value("offset", 0).toInt();
    _productIds = rawfile.value("product_ids").toStringList();
}
