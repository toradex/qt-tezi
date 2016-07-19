#include "progressslideshowdialog.h"
#include "ui_progressslideshowdialog.h"
#include <QDir>
#include <QFile>
#include <QPixmap>
#include <QDesktopWidget>
#include <QDebug>

/* Progress dialog with slideshow
 *
 * Initial author: Floris Bos
 * Maintained by Raspberry Pi
 *
 * See LICENSE.txt for license details
 *
 */

ProgressSlideshowDialog::ProgressSlideshowDialog(const QStringList &slidesDirectories, const QString &statusMsg, int changeInterval, QWidget *parent) :
    QDialog(parent),
    _pos(0),
    _changeInterval(changeInterval),
    _maxSectors(0),
    _lastBytes(0),
    ui(new Ui::ProgressSlideshowDialog)
{
    ui->setupUi(this);
    setLabelText(statusMsg);

    QRect s = QApplication::desktop()->screenGeometry();
    if (s.height() < 400)
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    else
        setWindowFlags(Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint);

    foreach (QString slidesDirectory, slidesDirectories)
    {
        QDir dir(slidesDirectory, "*.jpg *.jpeg *.png");
        if (dir.exists())
        {
            QStringList s = dir.entryList();
            s.sort();

            foreach (QString slide, s)
            {
                _slides.append(slidesDirectory+"/"+slide);
            }
        }
    }
    qDebug() << "Available slides" << _slides;

    if (_slides.isEmpty())
    {
        /* Resize window to just show progress bar */
        ui->imagespace->setMinimumSize(0, 0);
        resize(this->width(), 50);

    }
    else
    {
        /* Resize window to size of first image in slide directory */
        QPixmap pixmap(_slides.first());
        ui->imagespace->setMinimumSize(pixmap.width(), pixmap.height());
        resize(pixmap.width(), pixmap.height()+50);

        ui->imagespace->setPixmap(pixmap);

        connect(&_timer, SIGNAL(timeout()), this, SLOT(nextSlide()));
        _timer.start(changeInterval * 1000);
    }
    _t1.start();
}

ProgressSlideshowDialog::~ProgressSlideshowDialog()
{
    delete ui;
}

void ProgressSlideshowDialog::setLabelText(const QString &text)
{
    QString txt = text;
    txt.replace('\n',' ');
    ui->statusLabel->setText(txt);
    qDebug() << text;
}

void ProgressSlideshowDialog::nextSlide()
{
    if (++_pos >= _slides.size())
        _pos = 0;

    QString newSlide = _slides.at(_pos);
    if (QFile::exists(newSlide))
        ui->imagespace->setPixmap(QPixmap(newSlide));
}

void ProgressSlideshowDialog::setMaximum(qint64 bytes)
{
    _maxSectors = bytes / 512;
    ui->progressBar->setMaximum(_maxSectors);
}

void ProgressSlideshowDialog::updateIOstats(qint64 bytes)
{
    int sectors = bytes / 512;
    QString progress;

    if (_maxSectors)
    {
        progress = tr("%1 MB of %2 MB written")
                .arg(QString::number(sectors/2048), QString::number(_maxSectors/2048));

        sectors = qMin(_maxSectors, sectors);
        ui->progressBar->setValue(sectors);
    }
    else
    {
        progress = tr("%1 MB written")
                .arg(QString::number(sectors/2048));
    }

    // Update interval is 1 by default, so we can calculate current speed simply by using the difference
    // to the last value provided...
    if (_lastBytes) {
        QString speed = tr("(%3 MB/sec)").arg(QString::number((bytes - _lastBytes)/1024.0/1024.0, 'f', 1));
        progress += " " + speed;
    }

    _lastBytes = bytes;
    ui->mbwrittenLabel->setText(progress);
}
