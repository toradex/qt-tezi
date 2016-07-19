#ifndef PROGRESSSLIDESHOWDIALOG_H
#define PROGRESSSLIDESHOWDIALOG_H

/* Progress dialog with slideshow
 *
 * Initial author: Floris Bos
 * Maintained by Raspberry Pi
 *
 * See LICENSE.txt for license details
 *
 */

#include <QProgressDialog>
#include <QTimer>
#include <QTime>

namespace Ui {
class ProgressSlideshowDialog;
}

class ProgressSlideshowDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProgressSlideshowDialog(const QStringList &slidesDirectories, const QString &statusMsg = "", int changeInterval = 20, QWidget *parent = 0);
    ~ProgressSlideshowDialog();
    void enableIOaccounting();
    void disableIOaccounting();

public slots:
    void setLabelText(const QString &text);
    void setMaximum(qint64 bytes);
    void nextSlide();
    void updateIOstats(qint64 progress);

protected:
    QStringList _slides;
    int _pos, _changeInterval, _maxSectors;
    qint64 _lastBytes;
    QTimer _timer;
    QTime _t1;


private:
    Ui::ProgressSlideshowDialog *ui;
};

#endif // PROGRESSSLIDESHOWDIALOG_H
