#ifndef TWOICONSDELEGATE_H
#define TWOICONSDELEGATE_H

/* Delegate for a QListWidget that can draw a second icon
 * at the right side of the item
 *
 * Initial author: Floris Bos
 * Maintained by Raspberry Pi
 *
 * See LICENSE.txt for license details
 *
 */

#include <QStyledItemDelegate>

#define SecondIconRole  (Qt::UserRole+10)
#define NameRole		(Qt::UserRole+11)
#define VersionRole		(Qt::UserRole+12)
#define InfoRole	 	(Qt::UserRole+13)

class TwoIconsDelegate : public QStyledItemDelegate
{
    Q_OBJECT

    // Margins for text boxes
    const int _line_height = 13;
    const int _left_margin = 3;
    const int _title_top_margin = 5;
    const int _version_top_margin = _title_top_margin + _line_height;
    const int _info_top_margin = _version_top_margin + _line_height;

public:
    explicit TwoIconsDelegate(QObject *parent = 0);
    virtual void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

signals:

public slots:

};

#endif // TWOICONSDELEGATE_H
