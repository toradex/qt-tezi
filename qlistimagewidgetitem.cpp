#include "qlistimagewidgetitem.h"
#include <QDebug>

QListImageWidgetItem::QListImageWidgetItem(const QIcon &icon, const QString &text, int feedindex, int imageindex,
                                           QListWidget *view, int type)
 : QListWidgetItem(icon, text, view, type),
   _feedindex(feedindex),
   _imageindex(imageindex)
{

}

/* Provide sensible sort order for Tezi */
bool QListImageWidgetItem::operator<(const QListWidgetItem &other) const
{
    const QListImageWidgetItem *otherimage = dynamic_cast<const QListImageWidgetItem *>(&other);

    if (!otherimage)
        return false;

    if (_feedindex == otherimage->_feedindex)
        return _imageindex < otherimage->_imageindex;
    return _feedindex < otherimage->_feedindex;
}
