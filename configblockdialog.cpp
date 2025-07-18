#include "configblockdialog.h"
#include "configblock.h"
#include "ui_configblockdialog.h"

#include <QDebug>
#include <QKeyEvent>

ConfigBlockDialog::ConfigBlockDialog(QList<quint16> supportedModules, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigBlockDialog)
{
    ui->setupUi(this);

    ui->moduleType->installEventFilter(this);

    // Force User to choose a module, set to invalid value "Unknown Module"
    ui->moduleType->addItem(QString(toradex_modules[0].name), 0);

    foreach (quint16 productId, supportedModules) {
        int idx = configBlock->getToradexModuleIndex(productId);

        ui->moduleType->addItem(QString(toradex_modules[idx].name), productId);
    }
}

ConfigBlockDialog::~ConfigBlockDialog()
{
    delete ui;
}

bool ConfigBlockDialog::checkUserInput(quint16 productId, QLineEdit *moduleSerial, QLineEdit *moduleVersion)
{
    quint32 serial = moduleSerial->text().toUInt();
    QString version = moduleVersion->text();
    QByteArray asciiver = version.toLatin1();

    if (!productId)
        return false;

    if (!ui->moduleVersion->hasAcceptableInput())
        return false;

    if (!((asciiver[4] >= 'A' && asciiver[4] <= 'Z') || asciiver[4] == '#'))
        return false;

    if (!moduleSerial->hasAcceptableInput())
        return false;

    if (!ConfigBlock::isSerialValid(serial))
        return false;

    return true;
}

void ConfigBlockDialog::accept()
{
    quint16 productId = ui->moduleType->itemData(ui->moduleType->currentIndex()).toUInt();

    if (!checkUserInput(productId, ui->moduleSerial, ui->moduleVersion)) {
        QMessageBox::critical(NULL, QObject::tr("Config Block information not valid"),
                              QObject::tr("Please enter valid and complete Config Block data."),
                              QMessageBox::Close);
        return;
    }
    configBlock = ConfigBlock::configBlockFromUserInput(productId, ui->moduleVersion->text(), ui->moduleSerial->text());
    QDialog::accept();
}

bool ConfigBlockDialog::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() >= Qt::Key_0 && keyEvent->key() <= Qt::Key_9)
            cfgBlock.append(keyEvent->text());
        if (keyEvent->key() == Qt::Key_Return) {
            qDebug() << "Detected return, cfgBlock: " << cfgBlock;

            /*
             * If string we accumulated so far is too short return false
             * -> this return event won't close the dialog
             */
            if (cfgBlock.length() < 16)
                return true;

            /*
             * Convert string read from a scanner and fill text fields
             * e.g. "0032110202983024"
             */
            QString productId = cfgBlock.mid(0, 4);
            int idx = ui->moduleType->findData(productId.toInt());
            /*
             * If not found in module type combo box return false
             * -> this return event won't close the dialog
             */
            if (idx < 0) {
                    cfgBlock = "";
                    QMessageBox::critical(NULL, QObject::tr("Config Block information not valid"),
                                          QObject::tr("This product ID is not valid for the module Toradex Easy Installer is running on."),
                                          QMessageBox::Close);
                    return true;
            }
            ui->moduleType->setCurrentIndex(idx);

            QString version = QString("V%1.%2%3")
                .arg(cfgBlock.mid(4, 1).toInt())
                .arg(cfgBlock.mid(5, 1).toInt())
                .arg(QChar(cfgBlock.mid(7, 1).toInt() + 'A'));
            ui->moduleVersion->setText(version);

            ui->moduleSerial->setText(cfgBlock.mid(8, 8));

            cfgBlock = "";
            return false;
        }
        return false;
    }

    return QDialog::eventFilter(target, event);
}
