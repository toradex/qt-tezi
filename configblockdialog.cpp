#include "configblockdialog.h"
#include "configblock.h"
#include "ui_configblockdialog.h"

#include <QDebug>
#include <QMessageBox>

ConfigBlockDialog::ConfigBlockDialog(QList<quint16> supportedModules, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigBlockDialog)
{
    ui->setupUi(this);

    // Force User to choose a module, set to invalid value "Unknown Module"
    ui->moduleType->addItem(QString(toradex_modules[0]), 0);

    foreach (quint16 productId, supportedModules)
        ui->moduleType->addItem(QString(toradex_modules[productId]), productId);
}

ConfigBlockDialog::~ConfigBlockDialog()
{
    delete ui;
}

void ConfigBlockDialog::accept()
{
    quint16 productId = ui->moduleType->itemData(ui->moduleType->currentIndex()).toUInt();

    if (!productId || !ui->moduleVersion->hasAcceptableInput() ||
        !ui->moduleSerial->hasAcceptableInput()) {
        QMessageBox::critical(NULL, QObject::tr("Config Block information not valid"),
                              QObject::tr("Please enter valid and complete Config Block data."),
                              QMessageBox::Close);
        return;
    }
    configBlock = ConfigBlock::configBlockFromUserInput(productId, ui->moduleVersion->text(), ui->moduleSerial->text());
    QDialog::accept();
}
