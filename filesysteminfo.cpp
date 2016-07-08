#include "filesysteminfo.h"

FileSystemInfo::FileSystemInfo(const QVariantMap &m, QObject *parent) :
  QObject(parent)
{

    _fstype        = m.value("filesystem_type").toByteArray().toLower();
    _mkfsOptions   = m.value("mkfs_options").toByteArray();
    _ddOptions   = m.value("dd_options").toByteArray();
    _label         = m.value("label").toByteArray();
    _filename      = m.value("filename").toString();
    _uncompressedTarballSize = m.value("uncompressed_tarball_size").toInt();
}
