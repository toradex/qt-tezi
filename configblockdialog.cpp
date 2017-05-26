#include "configblockdialog.h"
#include "ui_configblockdialog.h"

#include <QDebug>

ConfigBlockDialog::ConfigBlockDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigBlockDialog)
{
    ui->setupUi(this);
}

ConfigBlockDialog::~ConfigBlockDialog()
{
    delete ui;
}

void ConfigBlockDialog::accept()
{
    qDebug() << ui->moduleSerial->text();
    QDialog::accept();
}
