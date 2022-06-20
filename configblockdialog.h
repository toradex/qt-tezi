#ifndef CONFIGBLOCKDIALOG_H
#define CONFIGBLOCKDIALOG_H

#include <QDialog>
#include <QMessageBox>
#include <QLineEdit>
#include "configblock.h"

namespace Ui {
class ConfigBlockDialog;
}

class ConfigBlockDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigBlockDialog(QList<quint16> supportedModules, QWidget *parent = 0);
    ~ConfigBlockDialog();

    ConfigBlock *configBlock;

public slots:
    virtual void accept();
    virtual bool eventFilter(QObject *target, QEvent *event);

private:
    Ui::ConfigBlockDialog *ui;
    QString cfgBlock;
    bool checkUserInput(quint16 productId, QLineEdit *moduleSerial, QLineEdit *moduleVersion);
};

#endif // CONFIGBLOCKDIALOG_H
