#ifndef IMAGESOURCE_H
#define IMAGESOURCE_H

#include <QObject>

enum ImageSource {
    SOURCE_UNKOWN,
    SOURCE_USB,
    SOURCE_SDCARD,
    SOURCE_NETWORK,
    SOURCE_INTERNET,
    SOURCE_NCM,
};
Q_DECLARE_METATYPE(ImageSource);

#endif // IMAGESOURCE_H
