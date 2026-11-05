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

#include <functional>
#include <QWidget>

#include "params.h"

class QFormLayout;

/**
 * Edits a ParamSet with a form generated from its parameter specs.
 */
class ParamForm : public QWidget
{
    Q_OBJECT
public:
    explicit ParamForm(QWidget *parent = nullptr);

    /**
     * Show these values. The form is only rebuilt if the spec list changed.
     */
    void setParams(const OnnxInf::ParamSet &params);
    [[nodiscard]] const OnnxInf::ParamSet &params() const;

signals:
    void paramsChanged();

private:
    struct Editor {
        std::string key;
        std::function<void()> load; /// show the current value of the key
    };

    void rebuild();
    void loadValues();
    void applyValue(QWidget *widget, const std::string &key, const OnnxInf::ParamValue &value);
    QWidget *createEditor(const OnnxInf::ParamSpec &spec);

    OnnxInf::ParamSet m_params;
    const std::vector<OnnxInf::ParamSpec> *m_shownSpecs{nullptr};
    QFormLayout *m_layout;
    std::vector<Editor> m_editors;
    bool m_loading{false};
};
