#ifndef MEDIAPOLLTHREAD_H
#define MEDIAPOLLTHREAD_H

#include "dto/imageinfo.h"
#include "moduleinformation.h"

#include <QProcess>
#include <QObject>
#include <QThread>
#include <QVariantMap>
#include <QSet>
#include <QMutex>

typedef QList<QVariantMap> QListVariantMap;

class QFileInfo;

class MediaPollThread : public QThread
{
    Q_OBJECT

public:
    MediaPollThread(ModuleInformation *moduleInformation, QObject *parent);

    void run();
    bool mountMedia(const QString &blockdev);
    bool unmountMedia();
    static int calculateNominalSize(const QVariantMap &imagemap);

protected:
    bool isMounted(const QString &path);

    void poll();
    void processMedia(enum ImageSource src, const QString &blockdev);
    void parseTeziConfig(const QString &path);
    void checkRemovableBlockdev(const QString &nameFilter);
    void checkSDcard();
    QList<QVariantMap> listMediaImages(const QString &path, const QString &blockdev, enum ImageSource source);
    QList<QFileInfo> findImages(const QString &path, int depth);

signals:
    void firstScanFinished();
    void finished();
    void errorMounting(const QString blockdev);

    void removedBlockdev(const QString blockdev);
    void newImageUrl(const QString url);
    void newImagesToAdd(const QListVariantMap images);

protected:
    ModuleInformation *_moduleInformation;
    QSet<QString> _blockdevsChecked;
    QSet<QString> _blockdevsChecking;
    QMutex mountMutex;
};

#endif // MEDIAPOLLTHREAD_H
