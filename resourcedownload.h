#ifndef RESOURCEDOWNLOAD_H
#define RESOURCEDOWNLOAD_H

#include <QObject>
#include <QNetworkAccessManager>

class ResourceDownload : public QObject
{
    Q_OBJECT
public:
    explicit ResourceDownload(QNetworkAccessManager *netaccess, const QString &urlstring, const QString &saveAs, QObject *parent = 0);
    int saveToFile();

    inline QString saveAs()
    {
        return _saveAs;
    }
    inline QString urlString()
    {
        return _urlString;
    }
    inline QByteArray data()
    {
        return _data;
    }

signals:
    void completed();
    void failed();

public slots:

protected slots:
    void downloadRedirectCheck();
private:
    void downloadFile(const QString &urlstring);

    QNetworkAccessManager *_netaccess;
    QString _saveAs;
    QString _urlString;
    QByteArray _data;
};

#endif // RESOURCEDOWNLOAD_H
