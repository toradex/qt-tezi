#ifndef QLISTIMAGEWIDGETITEM_H
#define QLISTIMAGEWIDGETITEM_H

#include <QListWidgetItem>

class QListImageWidgetItem : public QListWidgetItem
{
public:
    QListImageWidgetItem(const QIcon &icon, const QString &text, int feedindex, int imageindex,
                    QListWidget *view = 0, int type = Type);

    virtual bool operator<(const QListWidgetItem &other) const;

private:
    int _feedindex, _imageindex;
};

#endif // QLISTIMAGEWIDGETITEM_H
