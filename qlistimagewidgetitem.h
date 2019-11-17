#ifndef QLISTIMAGEWIDGETITEM_H
#define QLISTIMAGEWIDGETITEM_H

#include <QListWidgetItem>

class QListImageWidgetItem : public QListWidgetItem
{
public:
    QListImageWidgetItem(const QIcon &icon, const QString &text,
                    QListWidget *view = 0, int type = Type);

private:
};

#endif // QLISTIMAGEWIDGETITEM_H
