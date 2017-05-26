#ifndef CONFIGBLOCKDIALOG_H
#define CONFIGBLOCKDIALOG_H

#include <QDialog>
#include "configblock.h"

namespace Ui {
class ConfigBlockDialog;
}

class ConfigBlockDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigBlockDialog(QWidget *parent = 0);
    ~ConfigBlockDialog();

    ConfigBlock *configBlock;

public slots:
    virtual void accept();

private:
    Ui::ConfigBlockDialog *ui;
};

#endif // CONFIGBLOCKDIALOG_H
