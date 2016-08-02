#ifndef SCROLLTEXTDIALOG_H
#define SCROLLTEXTDIALOG_H
#include <QDialog>
#include <QString>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QLabel>
#include <QPushButton>

class ScrollTextDialog : public QDialog
{
    Q_OBJECT

public:
    ScrollTextDialog(QString const& title,
                     QString const& text, QDialogButtonBox::StandardButtons buttons,
                     QWidget* parent = 0);
    void setDefaultButton(QDialogButtonBox::StandardButton button);

private:
    void setDefaultButton(QPushButton *button);

    QLabel *label;
    QDialogButtonBox *buttonBox;


protected slots:
    void handle_buttonClicked(QAbstractButton *button);
};

#endif // SCROLLTEXTDIALOG_H
