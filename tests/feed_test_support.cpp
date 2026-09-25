#include "json.h"
#include "mediapollthread.h"
#include <QJsonDocument>

// Match Json::parse without linking the unrelated schema-validation dependencies.
QVariant Json::parse(const QByteArray &data)
{
    return QJsonDocument::fromJson(data).toVariant();
}

// Fixtures supply nominal_size; linking the media polling thread would pull in
// board hardware dependencies that these feed tests do not exercise.
int MediaPollThread::calculateNominalSize(const QVariantMap &)
{
    return 0;
}
