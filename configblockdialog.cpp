#include "configblockdialog.h"
#include "configblock.h"
#include "ui_configblockdialog.h"

#include <QDebug>
#include <QMessageBox>

ConfigBlockDialog::ConfigBlockDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigBlockDialog)
{
    ui->setupUi(this);
    //ui->moduleVersion->setCursorPosition(4);
    qDebug() << ui->moduleVersion->cursorPosition();
    int supported_modules[] = { 0, 27, 28, 29, 35, 14, 15, 16, 17, 32, 33 };

    for (unsigned int i=0; i < ARRAY_SIZE(supported_modules); i++) {
        quint16 productId = supported_modules[i];
        ui->moduleType->addItem(QString(toradex_modules[productId]), productId);
    }
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
