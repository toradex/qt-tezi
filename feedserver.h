#ifndef FEEDSERVER_H
#define FEEDSERVER_H

#include <QString>

typedef struct {
    QString label;
    QString url;
    bool enabled;
} FeedServer;

#endif // FEEDSERVER_H
