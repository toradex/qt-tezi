#ifndef FEEDSDIALOG_H
#define FEEDSDIALOG_H

#include <QDialog>
#include <QStringList>
#include <QList>
#include <QString>
#include "feedserver.h"

namespace Ui {
class FeedsDialog;
}

class FeedsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FeedsDialog(QList<FeedServer> &_networkFeedList, QWidget *parent = 0);
    ~FeedsDialog();

protected:
    QList<FeedServer> &_networkFeedServerList;
    void addFeedServer(const FeedServer &server);
    bool eventFilter(QObject *obj, QEvent *event);

private:
    Ui::FeedsDialog *ui;

public slots:
    virtual void accept();

private slots:
    void on_addPushButton_clicked();
};

#endif // FEEDSDIALOG_H
