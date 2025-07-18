#include "contentinfo.h"
#include "rawfileinfo.h"

ContentInfo::ContentInfo(const QVariantMap &m, QObject *parent) :
  QObject(parent), _fstype("raw")
{
    if (m.contains("filesystem_type"))
        _fstype = m.value("filesystem_type").toByteArray().toLower();

    _mkfsOptions   = m.value("mkfs_options").toByteArray();
    _label         = m.value("label").toByteArray();
    _filename      = m.value("filename").toString();
    _filelist      = m.value("filelist").toStringList();
    _uncompressedSize = m.value("uncompressed_size", 0).toInt();

    /* Parse raw files which should get written into this partition */
    QVariantList rawfiles = m.value("rawfiles").toList();
    foreach (QVariant rawfile, rawfiles) {
        QVariantMap rawfilemap = rawfile.toMap();
        _rawFiles.append(new RawFileInfo(rawfilemap, this));
    }

    /* We also allow a single raw file */
    if (m.contains("rawfile")) {
        QVariantMap rawfilemap = m.value("rawfile").toMap();
        _rawFiles.append(new RawFileInfo(rawfilemap, this));
    }
}
