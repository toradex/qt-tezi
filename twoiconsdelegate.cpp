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


QPixmap &TwoIconsDelegate::setAlpha(QPixmap &pix, int val) const
{
    QPixmap alpha = pix;
    QPainter p(&alpha);
    p.fillRect(alpha.rect(), QColor(val, val, val));
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.end();
    return pix;
}

TwoIconsDelegate::TwoIconsDelegate(QObject *parent, QListWidget *listwidget) :
    QStyledItemDelegate(parent)
{
    _listWidget = listwidget;
    _selectionModel = listwidget->selectionModel();
}

void TwoIconsDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QPen lineMarkedPen(QColor::fromRgb(0,90,131), 3, Qt::SolidLine);
    QPen fontMarkedPen(Qt::black, 1, Qt::SolidLine);
    QPen greyedMarkedPen(Qt::gray, 1, Qt::SolidLine);
    QPen gradientPen(Qt::white, 0, Qt::NoPen);

    QString imageTitle = index.data(NameRole).toString();
    QString imageVersion = index.data(VersionRole).toString();
    QString imageInfo = index.data(InfoRole).toString();
    QString imageURI = index.data(URIRole).toString();
    QVariant vIcon = index.data(Qt::DecorationRole);
    QIcon ic = vIcon.value<QIcon>();
    QRect r = option.rect;
    QFont itemFont = painter->font();

    // Background on selection
    if(option.state & QStyle::State_Selected){
        QLinearGradient gradientSelected(r.left(), r.top() - r.height()/2, r.right(), r.top() - r.height()/2);
        gradientSelected.setColorAt(0.0, QColor::fromRgb(255,255,255));
        gradientSelected.setColorAt(0.06, QColor::fromRgb(255,255,255));
        gradientSelected.setColorAt(0.8, QColor::fromRgb(27,134,183));
        gradientSelected.setColorAt(0.9, QColor::fromRgb(0,120,174));
        painter->setBrush(gradientSelected);
        painter->setPen(gradientPen);
        painter->drawRect(r);
    }
    painter->save();
    painter->setPen(fontMarkedPen);

    // Icon
    if (!ic.isNull()) {
        if(!(option.state & QStyle::State_Enabled)) {
            QPixmap pixIcon = ic.pixmap(option.decorationSize);
            ic = QIcon(setAlpha(pixIcon, 50));
        }
        ic.paint(painter, r, Qt::AlignVCenter|Qt::AlignLeft);
    }

    // Title
    if(option.state & QStyle::State_Enabled)
        painter->setPen(fontMarkedPen);
    else 
        painter->setPen(greyedMarkedPen);

    QSize iconSize = option.decorationSize;
    r = option.rect.adjusted(_left_margin + iconSize.width(), _title_top_margin, 0, -10);
    itemFont.setBold(true);
    painter->setFont(itemFont);
    painter->drawText(r.left(), r.top(), r.width(), r.height(), Qt::AlignLeft, imageTitle, &r);

    // Version
    r = option.rect.adjusted(_left_margin + iconSize.width(), _version_top_margin, 0, 0);
    itemFont.setBold(false);
    painter->setFont(itemFont);
    painter->drawText(r.left(), r.top(), r.width(), r.height(), Qt::AlignLeft, imageVersion, &r);

    // Info
    if(option.state & QStyle::State_Selected) {
        r = option.rect.adjusted(_left_margin + iconSize.width(), _info_top_margin, -iconSize.width(), 0);
        painter->drawText(r.left(), r.top(), r.width(), r.height(), Qt::AlignLeft | Qt::TextWordWrap, imageInfo + "\n" + imageURI, &r);
    }

    // Second Icon
    QVariant v = index.data(SecondIconRole);
    if (!v.isNull() && v.canConvert<QIcon>())
    {
        QIcon icon = v.value<QIcon>();
        QSize size = icon.availableSizes().first();
        if(!(option.state & QStyle::State_Enabled)) {
            QPixmap pixIcon = icon.pixmap(size);
            icon = QIcon(setAlpha(pixIcon, 50));
        }
        painter->drawPixmap(option.rect.right()-size.width(), option.rect.top(), icon.pixmap(size));
    }

    painter->restore();

}

QSize TwoIconsDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QFontMetrics fm(option.font);
    QString text;
    QSize iconSize = option.decorationSize;
    QRect r = option.rect.adjusted(_left_margin + iconSize.width(), 0, -iconSize.width(), 0);
    int textHeight;
    if (_selectionModel->isSelected(index)) {
        const QAbstractItemModel* model = index.model();
        text = model->data(index, NameRole).toString();
        textHeight = fm.boundingRect(r, Qt::TextWordWrap, text).height();
        text = model->data(index, VersionRole).toString();
        textHeight += fm.boundingRect(r, Qt::TextWordWrap, text).height();
        text = model->data(index, InfoRole).toString();
        textHeight += fm.boundingRect(r, Qt::TextWordWrap, text).height();
        text = model->data(index, URIRole).toString();
        textHeight += fm.boundingRect(r, Qt::TextWordWrap, text).height();
        return QSize(option.rect.width(), textHeight + _bottom_margin);
    }
    return QStyledItemDelegate::sizeHint(option, index);
}
