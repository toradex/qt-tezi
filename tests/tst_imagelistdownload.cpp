#include "imagelistdownload.h"
#include "imagelist.h"
#include "resourcedownload.h"

#include <QtTest>
#include <QJsonDocument>
#include <QPointer>
#include <QTemporaryDir>
#include <cstring>

class Reply : public QNetworkReply
{
    QByteArray _body;
    qint64 _offset = 0;
public:
    Reply(const QNetworkRequest &request, const QByteArray &body, int status, QObject *parent)
        : QNetworkReply(parent), _body(body)
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        open(QIODevice::ReadOnly);
    }
    void complete()
    {
        if (!isFinished()) {
            setFinished(true);
            emit finished();
        }
    }
    void abort() override
    {
        if (!isFinished()) {
            setError(OperationCanceledError, "cancelled");
            complete();
        }
    }
    qint64 bytesAvailable() const override
    {
        return _body.size() - _offset + QNetworkReply::bytesAvailable();
    }
    qint64 readData(char *data, qint64 max) override
    {
        const qint64 size = qMin(max, _body.size() - _offset);
        if (!size)
            return -1;
        memcpy(data, _body.constData() + _offset, size);
        _offset += size;
        return size;
    }
};

// Replies complete only when the test asks, in alternating order.
class Network : public QNetworkAccessManager
{
    QList<QPointer<Reply>> _pending;
    bool _reverse = false;
public:
    int count = 1000;
    bool badList = false;

    bool finishOne()
    {
        if (_pending.isEmpty())
            return false;
        _reverse = !_reverse;
        QPointer<Reply> reply = _reverse ? _pending.takeLast() : _pending.takeFirst();
        if (reply)
            reply->complete();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        return true;
    }
    bool finishAll()
    {
        for (int i = 0; i < 10000; ++i)
            if (!finishOne())
                return true;
        return false;
    }
protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request, QIODevice *) override
    {
        QByteArray body;
        int status = 200;
        if (request.url().path() == "/list") {
            QVariantList images;
            for (int i = 0; i < count; ++i)
                images.append(QString("%1/teziimage/%2image.json").arg(i).arg(QString()));
            QVariantMap map;
            map["images"] = images;
            body = QJsonDocument::fromVariant(map).toJson();
            if (badList)
                status = 500;
        } else if (request.url().path().endsWith("image.json")) {
            const int index = request.url().path().split('/').at(1).toInt();
            body = index == 7 ? QByteArray("invalid")
                             : QByteArray("{\"name\":\"test\",\"nominal_size\":0,\"supported_product_ids\":[\"00000000\"],\"icon\":\"icon.png\"}");
        } else {
            body = request.url().path().toUtf8();
            if (request.url().path().startsWith("/9/"))
                status = 500;
        }
        Reply *reply = new Reply(request, body, status, this);
        _pending.append(reply);
        return reply;
    }
};

class Owner : public QObject
{
    Q_OBJECT
signals:
    void abortAllDownloads();
};

struct Fixture
{
    QTemporaryDir storage;
    Network network;
    Owner owner;
    int finished = 0, published = 0;
    QStringList errors;
    QListVariantMap images;

    ImageListDownload *start(const QString &root = QString(), bool single = false)
    {
        const QString path = root.isNull() ? storage.path() : root;
        ImageListDownload *download = single
            ? new ImageListDownload("http://test/0/teziimage/image.json", SOURCE_NCM, &network, &owner, path)
            : new ImageListDownload("http://test/list", SOURCE_INTERNET, 0, &network, &owner, path);
        QObject::connect(download, &ImageListDownload::finished, &owner, [this]() { ++finished; });
        QObject::connect(download, &ImageListDownload::error, &owner, [this](QString error) { errors.append(error); });
        QObject::connect(download, &ImageListDownload::newImagesToAdd, &owner, [this](QListVariantMap result) {
            ++published;
            images = result;
        });
        return download;
    }
};

class FeedTest : public QObject
{
    Q_OBJECT
private slots:
    void longFeed()
    {
        Fixture f;
        const int images_on_feed = 2000;
        f.network.count = images_on_feed;
        QVERIFY(f.storage.isValid());
        f.start();
        QVERIFY(f.network.finishAll());
        QCOMPARE(f.finished, 1);
        QCOMPARE(f.published, 1);
        QCOMPARE(f.images.size(), images_on_feed);
        QVERIFY(f.errors.isEmpty());
        QSet<QString> folders;
        int previous = -1;
        for (const QVariantMap &image : f.images) {
            const int index = image.value("index").toInt();
            QVERIFY(index > previous);
            previous = index;
            const QString folder = image.value("folder").toString();
            folders.insert(folder);
            QFile description(folder + "/image.json");
            QVERIFY(description.open(QIODevice::ReadOnly));
            QJsonDocument json_doc = QJsonDocument::fromJson(description.readAll());
            // Entry 7 is malformed, inserting an empty entry on list
            if (index == 7) {
                QVERIFY(!json_doc.isObject());
                continue;
            }
            QVERIFY(json_doc.isObject());
            if (index == 9)
                QVERIFY(!image.contains("iconimage")); // A failed icon must not discard its image.
            else
                QCOMPARE(image.value("iconimage").toByteArray(),
                         QString("/%1/teziimage/icon.png").arg(index).toUtf8());
        }
        QCOMPARE(folders.size(), f.images.size());
    }

    void httpError()
    {
        Fixture f;
        f.network.badList = true;
        f.start();
        QVERIFY(f.network.finishAll());
        QCOMPARE(f.finished, 1);
        QCOMPARE(f.errors.size(), 1);
        QVERIFY(f.errors.first().contains("500"));
    }
};

QTEST_GUILESS_MAIN(FeedTest)
#include "tst_imagelistdownload.moc"
