/* Delegate for a QListWidget that can draw a second icon
 * at the right side of the item
 *
 * Initial author: Floris Bos
 * Maintained by Raspberry Pi
 *
 * See LICENSE.txt for license details
 *
 */

#include "twoiconsdelegate.h"
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QtDebug>


TwoIconsDelegate::TwoIconsDelegate(QObject *parent) :
    QStyledItemDelegate(parent)
{
}

void TwoIconsDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QPen lineMarkedPen(QColor::fromRgb(0,90,131), 3, Qt::SolidLine);
    QPen fontMarkedPen(Qt::black, 1, Qt::SolidLine);
    QPen gradientPen(Qt::white, 0, Qt::NoPen);

    QString imageTitle = index.data(NameRole).toString();
    QString imageVersion = index.data(VersionRole).toString();
    QString imageInfo = index.data(InfoRole).toString();
    QVariant vIcon = index.data(Qt::DecorationRole);
    QIcon ic = vIcon.value<QIcon>();
    QRect r = option.rect;

    // On selection
    if(option.state & QStyle::State_Selected){
        QLinearGradient gradientSelected(r.left(), r.top() - r.height()/2, r.right(), r.top() - r.height()/2);
        gradientSelected.setColorAt(0.0, QColor::fromRgb(255,255,255));
        gradientSelected.setColorAt(0.06, QColor::fromRgb(255,255,255));
        gradientSelected.setColorAt(0.8, QColor::fromRgb(27,134,183));
        gradientSelected.setColorAt(0.9, QColor::fromRgb(0,120,174));
        painter->setBrush(gradientSelected);
        painter->setPen(gradientPen);
        painter->drawRect(r);
    } else {
    }
    painter->setPen(fontMarkedPen);

    // Icon
    int imageSpace = 10;
    if (!ic.isNull()) {
        ic.paint(painter, r, Qt::AlignVCenter|Qt::AlignLeft);
    }

    painter->save();

    // Title
    QFont itemFont = painter->font();
    itemFont.setBold(true);
    painter->setFont(itemFont);

    r = option.rect.adjusted(_left_margin, _title_top_margin, 0, -10);
    QSize iconSize = option.decorationSize;
    painter->drawText(r.left() + iconSize.width() + imageSpace, r.top(), r.width(), r.height(), Qt::AlignLeft, imageTitle, &r);

    // Version
    r = option.rect.adjusted(_left_margin, _version_top_margin, 0, 0);
    itemFont.setBold(false);
    painter->setFont(itemFont);
    painter->drawText(r.left() + iconSize.width() + imageSpace, r.top(), r.width(), r.height(), Qt::AlignLeft, imageVersion, &r);

    // Info
    if(option.state & QStyle::State_Selected){
        r = option.rect.adjusted(_left_margin, _info_top_margin, 0, 0);
        painter->drawText(r.left() + iconSize.width() + imageSpace, r.top(), r.width(), r.height(), Qt::AlignLeft, imageInfo, &r);
    }

    // Second Icon
    QVariant v = index.data(SecondIconRole);
    if (!v.isNull() && v.canConvert<QIcon>())
    {
        QIcon icon = v.value<QIcon>();
        QSize size = icon.availableSizes().first();

        painter->drawPixmap(option.rect.right()-size.width(), option.rect.top(), icon.pixmap(size));
    }

    painter->restore();

}

QSize TwoIconsDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    int width = option.rect.width();
    int height = option.rect.height();
    if(option.state & QStyle::State_Selected) {
        return QStyledItemDelegate::sizeHint(option, index)*1.5;
    } else {
        return QStyledItemDelegate::sizeHint(option, index);
    }
}
