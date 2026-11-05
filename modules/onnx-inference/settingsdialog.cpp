/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "settingsdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "paramform.h"
#include "units.h"

using namespace OnnxInf;

namespace
{

QString shapeWithNames(const TensorInfo &ti)
{
    QStringList dims;
    for (size_t i = 0; i < ti.shape.size(); ++i) {
        if (ti.shape[i] >= 0) {
            dims.append(QString::number(ti.shape[i]));
            continue;
        }
        const auto name = i < ti.dimNames.size() ? QString::fromStdString(ti.dimNames[i]) : QString();
        dims.append(name.isEmpty() ? QStringLiteral("?") : name);
    }
    return QStringLiteral("[%1]").arg(dims.join(QStringLiteral(", ")));
}

QString providerShortName(const std::string &provider)
{
    auto name = QString::fromStdString(provider);
    if (name.endsWith(QStringLiteral("ExecutionProvider")))
        name.chop(QStringLiteral("ExecutionProvider").length());
    return name;
}

} // namespace

OnnxSettingsDialog::OnnxSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("ONNX Inference Settings"));
    resize(600, 640);

    auto mainLayout = new QVBoxLayout(this);

    // model file
    auto modelRow = new QHBoxLayout;
    m_modelEdit = new QLineEdit(this);
    m_modelEdit->setReadOnly(true);
    m_modelEdit->setPlaceholderText(QStringLiteral("No model selected"));
    m_browseButton = new QPushButton(QStringLiteral("Browse…"), this);
    modelRow->addWidget(new QLabel(QStringLiteral("Model:"), this));
    modelRow->addWidget(m_modelEdit, 1);
    modelRow->addWidget(m_browseButton);
    mainLayout->addLayout(modelRow);

    m_modelStatusLabel = new QLabel(this);
    m_modelStatusLabel->setWordWrap(true);
    m_modelStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    mainLayout->addWidget(m_modelStatusLabel);

    auto tabs = new QTabWidget(this);
    mainLayout->addWidget(tabs, 1);

    // recipe page
    m_recipePage = new QWidget(tabs);
    auto recipeLayout = new QVBoxLayout(m_recipePage);

    auto inputBox = new QGroupBox(QStringLiteral("Input"), m_recipePage);
    auto inputLayout = new QFormLayout(inputBox);
    m_adapterCombo = new QComboBox(inputBox);
    for (const auto &u : adapterUnits()) {
        m_adapterCombo->addItem(QString::fromStdString(u.title), QString::fromStdString(u.id));
        m_adapterCombo->setItemData(
            m_adapterCombo->count() - 1,
            QString::fromStdString(u.description),
            Qt::ToolTipRole);
    }
    m_inputForm = new ParamForm(inputBox);
    inputLayout->addRow(QStringLiteral("Adapter"), m_adapterCombo);
    inputLayout->addRow(m_inputForm);
    recipeLayout->addWidget(inputBox);

    auto outputBox = new QGroupBox(QStringLiteral("Output"), m_recipePage);
    auto outputLayout = new QFormLayout(outputBox);
    m_decoderCombo = new QComboBox(outputBox);
    for (const auto &u : decoderUnits()) {
        m_decoderCombo->addItem(QString::fromStdString(u.title), QString::fromStdString(u.id));
        m_decoderCombo->setItemData(
            m_decoderCombo->count() - 1,
            QString::fromStdString(u.description),
            Qt::ToolTipRole);
    }
    m_outputForm = new ParamForm(outputBox);
    outputLayout->addRow(QStringLiteral("Decoder"), m_decoderCombo);
    outputLayout->addRow(m_outputForm);
    recipeLayout->addWidget(outputBox);

    m_notesLabel = new QLabel(m_recipePage);
    m_notesLabel->setWordWrap(true);
    m_notesLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    recipeLayout->addWidget(m_notesLabel);

    auto autoFillRow = new QHBoxLayout;
    m_autoFillButton = new QPushButton(QStringLiteral("Fill in from model"), m_recipePage);
    m_autoFillButton->setToolTip(QStringLiteral(
        "Replace the recipe with the one next to the model file (<model>.recipe.toml), "
        "or with one derived from the model's tensor shapes."));
    autoFillRow->addStretch();
    autoFillRow->addWidget(m_autoFillButton);
    recipeLayout->addLayout(autoFillRow);
    recipeLayout->addStretch();
    tabs->addTab(m_recipePage, QStringLiteral("Recipe"));

    // model details page
    m_infoTree = new QTreeWidget(tabs);
    m_infoTree->setColumnCount(3);
    m_infoTree->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("Type"), QStringLiteral("Shape / Value")});
    m_infoTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_infoTree->setRootIsDecorated(true);
    tabs->addTab(m_infoTree, QStringLiteral("Model Details"));

    // engine page
    m_enginePage = new QWidget(tabs);
    auto engineLayout = new QFormLayout(m_enginePage);
    m_providerCombo = new QComboBox(m_enginePage);
    auto providerModel = qobject_cast<QStandardItemModel *>(m_providerCombo->model());
    for (const auto &p : OrtEngine::availableProviders()) {
        const auto name = providerShortName(p);
        m_providerCombo->addItem(name, name);
        if (name != QStringLiteral("CPU") && providerModel != nullptr) {
            auto item = providerModel->item(m_providerCombo->count() - 1);
            item->setEnabled(false);
            item->setToolTip(QStringLiteral("Not supported by this module yet."));
        }
    }
    if (m_providerCombo->findData(QStringLiteral("CPU")) < 0)
        m_providerCombo->addItem(QStringLiteral("CPU"), QStringLiteral("CPU"));

    m_threadsSpin = new QSpinBox(m_enginePage);
    m_threadsSpin->setRange(1, 64);
    m_threadsSpin->setToolTip(QStringLiteral("Threads ONNX Runtime may use for a single inference."));

    m_optLevelCombo = new QComboBox(m_enginePage);
    for (const auto level :
         {GraphOptLevel::All, GraphOptLevel::Extended, GraphOptLevel::Basic, GraphOptLevel::Disabled})
        m_optLevelCombo->addItem(
            QString::fromStdString(graphOptLevelToString(level)),
            QString::fromStdString(graphOptLevelToString(level)));

    engineLayout->addRow(QStringLiteral("Execution provider"), m_providerCombo);
    engineLayout->addRow(QStringLiteral("Threads"), m_threadsSpin);
    engineLayout->addRow(QStringLiteral("Graph optimization"), m_optLevelCombo);
    engineLayout->addRow(new QLabel(
        QStringLiteral("ONNX Runtime %1").arg(QString::fromStdString(OrtEngine::ortVersion())),
        m_enginePage));
    tabs->addTab(m_enginePage, QStringLiteral("Engine"));

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    mainLayout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);
    connect(m_browseButton, &QPushButton::clicked, this, &OnnxSettingsDialog::browseModel);
    connect(m_autoFillButton, &QPushButton::clicked, this, &OnnxSettingsDialog::autoFillRequested);

    connect(m_adapterCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_loading)
            return;
        m_recipe.setAdapter(m_adapterCombo->currentData().toString().toStdString());
        m_inputForm->setParams(m_recipe.input);
        emitRecipe();
    });
    connect(m_decoderCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (m_loading)
            return;
        m_recipe.setDecoder(m_decoderCombo->currentData().toString().toStdString());
        m_outputForm->setParams(m_recipe.output);
        emitRecipe();
    });
    connect(m_inputForm, &ParamForm::paramsChanged, this, [this]() {
        m_recipe.input = m_inputForm->params();
        emitRecipe();
    });
    connect(m_outputForm, &ParamForm::paramsChanged, this, [this]() {
        m_recipe.output = m_outputForm->params();
        emitRecipe();
    });

    connect(m_providerCombo, &QComboBox::currentIndexChanged, this, &OnnxSettingsDialog::emitEngineOptions);
    connect(m_threadsSpin, &QSpinBox::valueChanged, this, &OnnxSettingsDialog::emitEngineOptions);
    connect(m_optLevelCombo, &QComboBox::currentIndexChanged, this, &OnnxSettingsDialog::emitEngineOptions);

    setModelInfo(nullptr);
}

void OnnxSettingsDialog::setModelFile(const QString &path)
{
    m_modelEdit->setText(path);
    m_modelEdit->setToolTip(path);
}

void OnnxSettingsDialog::setModelInfoLoading()
{
    m_modelStatusLabel->setText(QStringLiteral("Reading model…"));
    m_infoTree->clear();
}

void OnnxSettingsDialog::setModelInfo(const ModelInfo *info, const QString &error)
{
    m_infoTree->clear();
    if (info == nullptr) {
        if (error.isEmpty())
            m_modelStatusLabel->setText(
                m_modelEdit->text().isEmpty() ? QStringLiteral("Select an ONNX model file.") : QString());
        else
            m_modelStatusLabel->setText(
                QStringLiteral("<span style=\"color:red\">%1</span>").arg(error.toHtmlEscaped()));
        return;
    }

    auto summary = QStringLiteral("%1 input(s), %2 output(s)").arg(info->inputs.size()).arg(info->outputs.size());
    if (!info->producerName.empty())
        summary += QStringLiteral(" · made by %1").arg(QString::fromStdString(info->producerName));
    m_modelStatusLabel->setText(summary);

    const auto addTensors = [this](const QString &title, const std::vector<TensorInfo> &tensors) {
        auto group = new QTreeWidgetItem(m_infoTree, {title});
        for (const auto &t : tensors)
            new QTreeWidgetItem(
                group,
                {QString::fromStdString(t.name), QString::fromStdString(dtypeToString(t.dtype)), shapeWithNames(t)});
        group->setExpanded(true);
    };
    addTensors(QStringLiteral("Inputs"), info->inputs);
    addTensors(QStringLiteral("Outputs"), info->outputs);

    if (!info->metadata.empty()) {
        auto group = new QTreeWidgetItem(m_infoTree, {QStringLiteral("Metadata")});
        for (const auto &[key, value] : info->metadata) {
            const auto full = QString::fromStdString(value);
            auto item = new QTreeWidgetItem(group, {QString::fromStdString(key), QString(), full.left(200)});
            item->setToolTip(2, full.left(4000));
        }
        group->setExpanded(true);
    }

    auto group = new QTreeWidgetItem(m_infoTree, {QStringLiteral("Model")});
    const auto addInfo = [group](const QString &key, const std::string &value) {
        if (!value.empty())
            new QTreeWidgetItem(group, {key, QString(), QString::fromStdString(value)});
    };
    addInfo(QStringLiteral("Producer"), info->producerName);
    addInfo(QStringLiteral("Graph"), info->graphName);
    addInfo(QStringLiteral("Domain"), info->domain);
    addInfo(QStringLiteral("Description"), info->description);
    if (info->version > 0)
        addInfo(QStringLiteral("Version"), std::to_string(info->version));
    group->setExpanded(true);
}

void OnnxSettingsDialog::setRecipe(const Recipe &recipe)
{
    m_loading = true;
    m_recipe = recipe;
    m_adapterCombo->setCurrentIndex(m_adapterCombo->findData(QString::fromStdString(recipe.adapter)));
    m_decoderCombo->setCurrentIndex(m_decoderCombo->findData(QString::fromStdString(recipe.decoder)));
    m_inputForm->setParams(recipe.input);
    m_outputForm->setParams(recipe.output);
    m_loading = false;
}

void OnnxSettingsDialog::setRecipeNotes(const QStringList &notes)
{
    m_notesLabel->setText(notes.join(QLatin1Char('\n')));
    m_notesLabel->setVisible(!notes.isEmpty());
}

void OnnxSettingsDialog::setEngineOptions(const EngineOptions &options)
{
    m_loading = true;
    const auto providerIdx = m_providerCombo->findData(QString::fromStdString(options.provider));
    m_providerCombo->setCurrentIndex(providerIdx >= 0 ? providerIdx : m_providerCombo->findData(QStringLiteral("CPU")));
    m_threadsSpin->setValue(options.intraOpThreads);
    m_optLevelCombo->setCurrentIndex(
        m_optLevelCombo->findData(QString::fromStdString(graphOptLevelToString(options.optLevel))));
    m_loading = false;
}

void OnnxSettingsDialog::setRunning(bool running)
{
    m_browseButton->setEnabled(!running);
    m_recipePage->setEnabled(!running);
    m_enginePage->setEnabled(!running);
}

void OnnxSettingsDialog::browseModel()
{
    const QFileInfo current(m_modelEdit->text());
    auto dlg = new QFileDialog(
        this,
        QStringLiteral("Select ONNX Model"),
        current.exists() ? current.absolutePath() : QString(),
        QStringLiteral("ONNX models (*.onnx);;All files (*)"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setFileMode(QFileDialog::ExistingFile);
    connect(dlg, &QFileDialog::fileSelected, this, [this](const QString &file) {
        setModelFile(file);
        emit modelFileSelected(file);
    });

    // open() instead of exec(): exec() would run a nested event loop
    dlg->open();
}

void OnnxSettingsDialog::emitRecipe()
{
    if (m_loading)
        return;
    emit recipeChanged(m_recipe);
}

void OnnxSettingsDialog::emitEngineOptions()
{
    if (m_loading)
        return;

    EngineOptions options;
    options.provider = m_providerCombo->currentData().toString().toStdString();
    options.intraOpThreads = m_threadsSpin->value();
    options.optLevel = graphOptLevelFromString(m_optLevelCombo->currentData().toString().toStdString());
    emit engineOptionsChanged(options);
}
