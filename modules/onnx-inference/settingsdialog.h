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

#pragma once

#include <QDialog>

#include "ortengine.h"
#include "recipe.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTreeWidget;
class ParamForm;

/**
 * Settings window of the ONNX inference module.
 *
 * It never blocks: the window and its file picker are not modal, because a nested
 * event loop would stop the module from servicing its link to Syntalos.
 */
class OnnxSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OnnxSettingsDialog(QWidget *parent = nullptr);

    void setModelFile(const QString &path);
    void setModelInfoLoading();
    void setModelInfo(const OnnxInf::ModelInfo *info, const QString &error = {});
    void setRecipe(const OnnxInf::Recipe &recipe);
    void setRecipeNotes(const QStringList &notes);
    void setEngineOptions(const OnnxInf::EngineOptions &options);
    void setRunning(bool running);

signals:
    void modelFileSelected(const QString &path);
    void recipeChanged(const OnnxInf::Recipe &recipe);
    void engineOptionsChanged(const OnnxInf::EngineOptions &options);
    void autoFillRequested();

private:
    void browseModel();
    void emitRecipe();
    void emitEngineOptions();

    QLineEdit *m_modelEdit;
    QPushButton *m_browseButton;
    QLabel *m_modelStatusLabel;

    QComboBox *m_adapterCombo;
    QComboBox *m_decoderCombo;
    ParamForm *m_inputForm;
    ParamForm *m_outputForm;
    QLabel *m_notesLabel;
    QPushButton *m_autoFillButton;

    QTreeWidget *m_infoTree;

    QComboBox *m_providerCombo;
    QSpinBox *m_threadsSpin;
    QComboBox *m_optLevelCombo;

    QWidget *m_recipePage;
    QWidget *m_enginePage;
    OnnxInf::Recipe m_recipe;
    bool m_loading{false};
};
