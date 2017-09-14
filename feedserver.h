#ifndef FEEDSERVER_H
#define FEEDSERVER_H

#include <QString>

struct FeedServer {
    QString label;
    QString url;
    bool enabled;
    bool operator==(const FeedServer& rhs) { return rhs.url == url; }
};

#endif // FEEDSERVER_H
