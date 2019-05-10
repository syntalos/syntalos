#ifndef LEDTRACKERSETTINGSDIALOG_H
#define LEDTRACKERSETTINGSDIALOG_H

#include <QDialog>

#include "imagesourcemodule.h"

namespace Ui {
class LedTrackerSettingsDialog;
}

class LedTrackerSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LedTrackerSettingsDialog(QWidget *parent = nullptr);
    ~LedTrackerSettingsDialog();

    void setRunning(bool running);

    QString resultsName() const;
    void setResultsName(const QString& name);

    void setImageSourceModules(const QList<ImageSourceModule *> &mods);
    ImageSourceModule *selectedImageSourceMod();
    void setSelectedImageSourceMod(ImageSourceModule *mod);

private slots:
    void on_frameSourceComboBox_currentIndexChanged(int index);
    void on_nameLineEdit_textChanged(const QString &arg1);

private:
    Ui::LedTrackerSettingsDialog *ui;

    QString m_resultsName;
    ImageSourceModule *m_selectedImgSrcMod;
};

#endif // LEDTRACKERSETTINGSDIALOG_H
