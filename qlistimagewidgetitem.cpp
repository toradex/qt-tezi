#include "qlistimagewidgetitem.h"
#include <QDebug>

QListImageWidgetItem::QListImageWidgetItem(const QIcon &icon, const QString &text,
                                           QListWidget *view, int type)
 : QListWidgetItem(icon, text, view, type)
{

}
