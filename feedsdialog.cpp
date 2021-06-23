#include "feedsdialog.h"
#include "ui_feedsdialog.h"

#include <QDebug>
#include <QKeyEvent>
#include <QEvent>
#include "config.h"
Q_DECLARE_METATYPE(FeedServer);

FeedsDialog::FeedsDialog(QList<FeedServer> &_networkFeedServerList, QWidget *parent) :
    QDialog(parent),
    _networkFeedServerList(_networkFeedServerList),
    ui(new Ui::FeedsDialog)
{
    ui->setupUi(this);
    ui->serverListWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    /* Force focus to widget so that we can use keyboard for selection */
    ui->serverListWidget->setFocus();

    foreach (const FeedServer server, _networkFeedServerList)
        addFeedServer(server);

    this->installEventFilter(this);
}

FeedsDialog::~FeedsDialog()
{
    delete ui;
}

bool FeedsDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (!ui->newServerLineEdit->hasFocus()) {
	    // Toggling feed checkboxes with pressing number keys should work
	    // only if the Input Field "Server Line" is not active
	    if (keyEvent->key() >= Qt::Key_1 && keyEvent->key() <= Qt::Key_9) {
                int index = keyEvent->key() - Qt::Key_1;
                if (ui->serverListWidget->count() > index) {
                    QListWidgetItem *item = ui->serverListWidget->item(index);
                    if (item->checkState() == Qt::Unchecked)
                        item->setCheckState(Qt::Checked);
                    else
                        item->setCheckState(Qt::Unchecked);
                }
            }
        }
    }
    return QObject::eventFilter(obj, event);
}

void FeedsDialog::addFeedServer(const FeedServer &server)
{
    QString text = QString("(%1) %2\n%3").arg(QString::number(ui->serverListWidget->count() + 1), server.label, server.url);
    QListWidgetItem* item = new QListWidgetItem(text, ui->serverListWidget);

    // Don't add empty server url
    if (server.url.isEmpty())
        return;

    item->setFlags(item->flags() | Qt::ItemIsUserCheckable); // set checkable flag
    if (server.enabled)
        item->setCheckState(Qt::Checked);
    else
        item->setCheckState(Qt::Unchecked);

    QVariant userData;
    userData.setValue(server);
    item->setData(Qt::UserRole, userData);
    ui->serverListWidget->addItem(item);
}

void FeedsDialog::on_addPushButton_clicked(void)
{
    FeedServer server;
    server.label = tr("Custom Server");
    server.url = ui->newServerLineEdit->text();
    server.enabled = true;

    // Classify as network by default. This could be a public or
    // a local url, we can't tell...
    if (server.url.contains(RNDIS_ADDRESS))
        server.source = SOURCE_RNDIS;
    else
        server.source = SOURCE_NETWORK;

    addFeedServer(server);

    ui->newServerLineEdit->clear();
}

void FeedsDialog::accept()
{
    _networkFeedServerList.clear();
    for (int i = 0; i < ui->serverListWidget->count(); i++)
    {
        QListWidgetItem *item = ui->serverListWidget->item(i);
        FeedServer server = item->data(Qt::UserRole).value<FeedServer>();
        server.enabled = item->checkState();
        _networkFeedServerList.append(server);
    }
    QDialog::accept();
}
