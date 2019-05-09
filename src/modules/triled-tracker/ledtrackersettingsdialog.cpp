#include "ledtrackersettingsdialog.h"
#include "ui_ledtrackersettingsdialog.h"

LedTrackerSettingsDialog::LedTrackerSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LedTrackerSettingsDialog),
    m_selectedImgSrcMod(nullptr)
{
    ui->setupUi(this);
}

LedTrackerSettingsDialog::~LedTrackerSettingsDialog()
{
    delete ui;
}

QString LedTrackerSettingsDialog::resultsName() const
{
    return m_resultsName;
}

void LedTrackerSettingsDialog::setResultsName(const QString &name)
{
    m_resultsName = name.simplified().replace(" ", "_");
    ui->nameLineEdit->setText(m_resultsName);
}

void LedTrackerSettingsDialog::setImageSourceModules(const QList<ImageSourceModule *> &mods)
{
    auto mod = m_selectedImgSrcMod;

    ui->frameSourceComboBox->clear();
    Q_FOREACH(auto mod, mods) {
        ui->frameSourceComboBox->addItem(mod->name(), QVariant(QMetaType::QObjectStar, &mod));
    }

    // ensure the right module is still selected
    for (int i = 0; i < ui->frameSourceComboBox->count(); i++) {
        if (ui->frameSourceComboBox->itemData(i).value<ImageSourceModule*>() == mod) {
            ui->frameSourceComboBox->setCurrentIndex(i);
            break;
        }
    }
    m_selectedImgSrcMod = mod;
}

ImageSourceModule *LedTrackerSettingsDialog::selectedImageSourceMod()
{
    return m_selectedImgSrcMod;
}

void LedTrackerSettingsDialog::setSelectedImageSourceMod(ImageSourceModule *mod)
{
    m_selectedImgSrcMod = mod;
}

void LedTrackerSettingsDialog::on_nameLineEdit_textChanged(const QString &arg1)
{
    m_resultsName = arg1.simplified().replace(" ", "_");
}

void LedTrackerSettingsDialog::on_frameSourceComboBox_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    m_selectedImgSrcMod = ui->frameSourceComboBox->currentData().value<ImageSourceModule*>();
}
