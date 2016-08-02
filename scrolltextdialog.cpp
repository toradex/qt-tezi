#include "scrolltextdialog.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QtGui/QScrollArea>
#include <QGridLayout>
#include <QStyle>
#include <QDebug>
#include <QSizePolicy>

ScrollTextDialog::ScrollTextDialog(QString const& title,
                                   QString const& text, QDialogButtonBox::StandardButtons buttons,
                                   QWidget* parent /*= 0*/) :
    QDialog(parent, Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint)
{
    QScrollArea *scroll;

    label = new QLabel;
    label->setTextInteractionFlags(Qt::TextInteractionFlags(style()->styleHint(QStyle::SH_MessageBox_TextInteractionFlags, 0, this)));
    label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    label->setOpenExternalLinks(true);
    label->setContentsMargins(2, 0, 0, 0);
    label->setIndent(9);
    label->setWordWrap(true);

    scroll = new QScrollArea(this);
    scroll->setGeometry(QRect(10, 20, 600, 430));
    scroll->setWidget(label);
    scroll->setWidgetResizable(true);

    buttonBox = new QDialogButtonBox(buttons);
    buttonBox->button(QDialogButtonBox::Yes)->setText(tr("I Agree"));
    buttonBox->setCenterButtons(style()->styleHint(QStyle::SH_MessageBox_CenterButtons, 0, this));
    QObject::connect(buttonBox, SIGNAL(clicked(QAbstractButton*)),
        this, SLOT(handle_buttonClicked(QAbstractButton*)));

    QGridLayout *grid = new QGridLayout;

    grid->addWidget(scroll, 0, 1, 1, 1);
    grid->addWidget(buttonBox, 1, 0, 1, 2);
    grid->setSizeConstraint(QLayout::SetNoConstraint);
    setLayout(grid);

    if (!title.isEmpty() || !text.isEmpty())
    {
      setWindowTitle(title);
      label->setText(text);
    }

    resize(600, 440);

    setModal(true);
}

void ScrollTextDialog::setDefaultButton(QPushButton *button)
{
    if (!buttonBox->buttons().contains(button))
        return;
    button->setDefault(true);
    button->setFocus();
}

void ScrollTextDialog::handle_buttonClicked(QAbstractButton *button)
{
  int ret = buttonBox->standardButton(button);
  done(ret);
}

void ScrollTextDialog::setDefaultButton(QDialogButtonBox::StandardButton button)
{
    setDefaultButton(buttonBox->button(button));
}
