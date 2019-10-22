#ifndef FEEDSERVER_H
#define FEEDSERVER_H

#include <QString>
#include "imagesource.h"

struct FeedServer {
    QString label;
    QString url;
    bool enabled;
    enum ImageSource source;
    bool operator==(const FeedServer& rhs) { return rhs.url == url; }
};

#endif // FEEDSERVER_H
