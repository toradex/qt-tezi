#ifndef MTDNAMEDEVICETRANSLATOR_H
#define MTDNAMEDEVICETRANSLATOR_H

#include <QObject>
#include <QMap>

class MtdNameDeviceTranslator : public QObject
{
    Q_OBJECT
public:
    explicit MtdNameDeviceTranslator(QObject *parent = 0);

    QString translate(QString &name);

protected:
    QMap<QString, QString> _nameDeviceMap;
};

#endif // MTDNAMEDEVICETRANSLATOR_H
