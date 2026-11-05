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

#include "paramform.h"

#include <algorithm>
#include <limits>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSpinBox>

using namespace OnnxInf;

namespace
{

int clampToInt(double v)
{
    return static_cast<int>(std::clamp(
        v,
        static_cast<double>(std::numeric_limits<int>::min()),
        static_cast<double>(std::numeric_limits<int>::max())));
}

void markInvalid(QWidget *widget, bool invalid, const QString &reason = {})
{
    if (!invalid) {
        // an empty palette resolves nothing itself, so the widget follows the application palette again
        widget->setPalette(QPalette());
        return;
    }

    auto pal = widget->palette();
    pal.setColor(QPalette::Text, Qt::red);
    widget->setPalette(pal);
    widget->setToolTip(reason);
}

} // namespace

ParamForm::ParamForm(QWidget *parent)
    : QWidget(parent),
      m_layout(new QFormLayout(this))
{
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
}

void ParamForm::setParams(const ParamSet &params)
{
    const bool specsChanged = m_shownSpecs != &params.specs();
    m_params = params;
    if (specsChanged)
        rebuild();
    else
        loadValues();
}

const ParamSet &ParamForm::params() const
{
    return m_params;
}

void ParamForm::rebuild()
{
    while (m_layout->rowCount() > 0)
        m_layout->removeRow(0);
    m_editors.clear();

    m_shownSpecs = &m_params.specs();
    for (const auto &spec : m_params.specs()) {
        const auto help = QString::fromStdString(spec.help);
        auto label = new QLabel(QString::fromStdString(spec.title), this);
        auto editor = createEditor(spec);
        label->setToolTip(help);
        editor->setToolTip(help);
        m_layout->addRow(label, editor);
    }

    loadValues();
}

void ParamForm::loadValues()
{
    m_loading = true;
    for (const auto &e : m_editors)
        e.load();
    m_loading = false;
}

void ParamForm::applyValue(QWidget *widget, const std::string &key, const ParamValue &value)
{
    if (m_loading)
        return;

    const auto res = m_params.set(key, value);
    if (!res) {
        markInvalid(widget, true, QString::fromStdString(res.error()));
        return;
    }

    markInvalid(widget, false);
    if (const auto spec = m_params.spec(key))
        widget->setToolTip(QString::fromStdString(spec->help));
    emit paramsChanged();
}

QWidget *ParamForm::createEditor(const ParamSpec &spec)
{
    const auto key = spec.key;

    switch (spec.type) {
    case ParamType::Bool: {
        auto cb = new QCheckBox(this);
        m_editors.push_back({key, [this, cb, key]() {
                                 cb->setChecked(m_params.getBool(key));
                             }});
        connect(cb, &QCheckBox::toggled, this, [this, cb, key](bool checked) {
            applyValue(cb, key, checked);
        });
        return cb;
    }

    case ParamType::Int: {
        auto sb = new QSpinBox(this);
        sb->setRange(clampToInt(spec.minValue), clampToInt(spec.maxValue));
        m_editors.push_back({key, [this, sb, key]() {
                                 sb->setValue(static_cast<int>(m_params.getInt(key)));
                             }});
        connect(sb, &QSpinBox::valueChanged, this, [this, sb, key](int value) {
            applyValue(sb, key, static_cast<int64_t>(value));
        });
        return sb;
    }

    case ParamType::Float: {
        auto sb = new QDoubleSpinBox(this);
        sb->setDecimals(4);
        sb->setSingleStep(0.05);
        sb->setRange(std::max(spec.minValue, -1e9), std::min(spec.maxValue, 1e9));
        m_editors.push_back({key, [this, sb, key]() {
                                 sb->setValue(m_params.getFloat(key));
                             }});
        connect(sb, &QDoubleSpinBox::valueChanged, this, [this, sb, key](double value) {
            applyValue(sb, key, value);
        });
        return sb;
    }

    case ParamType::String: {
        auto le = new QLineEdit(this);
        m_editors.push_back({key, [this, le, key]() {
                                 le->setText(QString::fromStdString(m_params.getString(key)));
                             }});
        connect(le, &QLineEdit::textEdited, this, [this, le, key](const QString &text) {
            applyValue(le, key, text.trimmed().toStdString());
        });
        return le;
    }

    case ParamType::Choice: {
        auto combo = new QComboBox(this);
        for (const auto &c : spec.choices)
            combo->addItem(QString::fromStdString(c.title), QString::fromStdString(c.value));
        m_editors.push_back({key, [this, combo, key]() {
                                 combo->setCurrentIndex(
                                     combo->findData(QString::fromStdString(m_params.getString(key))));
                             }});
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo, key](int) {
            applyValue(combo, key, combo->currentData().toString().toStdString());
        });
        return combo;
    }

    case ParamType::Size: {
        auto box = new QWidget(this);
        auto hbox = new QHBoxLayout(box);
        hbox->setContentsMargins(0, 0, 0, 0);
        auto wSpin = new QSpinBox(box);
        auto hSpin = new QSpinBox(box);
        for (auto sb : {wSpin, hSpin}) {
            sb->setRange(clampToInt(spec.minValue), clampToInt(spec.maxValue));
            sb->setSuffix(QStringLiteral(" px"));
        }
        hbox->addWidget(wSpin);
        hbox->addWidget(new QLabel(QStringLiteral("×"), box));
        hbox->addWidget(hSpin);
        hbox->addStretch();

        m_editors.push_back({key, [this, wSpin, hSpin, key]() {
                                 const auto size = m_params.getSize(key);
                                 wSpin->setValue(static_cast<int>(size[0]));
                                 hSpin->setValue(static_cast<int>(size[1]));
                             }});
        const auto apply = [this, box, wSpin, hSpin, key](int) {
            applyValue(box, key, std::vector<int64_t>{wSpin->value(), hSpin->value()});
        };
        connect(wSpin, &QSpinBox::valueChanged, this, apply);
        connect(hSpin, &QSpinBox::valueChanged, this, apply);
        return box;
    }

    case ParamType::FloatList: {
        auto le = new QLineEdit(this);
        le->setPlaceholderText(QStringLiteral("Numbers, separated by commas"));
        m_editors.push_back({key, [this, le, key]() {
                                 QStringList parts;
                                 for (const auto v : m_params.getFloatList(key))
                                     parts.append(QString::number(v));
                                 le->setText(parts.join(QStringLiteral(", ")));
                             }});
        connect(le, &QLineEdit::textEdited, this, [this, le, key](const QString &text) {
            std::vector<double> values;
            static const QRegularExpression sepRe(QStringLiteral("[,\\s]+"));
            for (const auto &part : text.split(sepRe, Qt::SkipEmptyParts)) {
                bool ok = false;
                const auto v = part.toDouble(&ok);
                if (!ok) {
                    markInvalid(le, true, QStringLiteral("'%1' is not a number.").arg(part));
                    return;
                }
                values.push_back(v);
            }
            applyValue(le, key, values);
        });
        return le;
    }

    case ParamType::StringList: {
        auto te = new QPlainTextEdit(this);
        te->setPlaceholderText(QStringLiteral("One entry per line"));
        te->setTabChangesFocus(true);
        te->setMaximumHeight(te->fontMetrics().lineSpacing() * 6 + 12);
        m_editors.push_back({key, [this, te, key]() {
                                 QStringList lines;
                                 for (const auto &s : m_params.getStringList(key))
                                     lines.append(QString::fromStdString(s));
                                 te->setPlainText(lines.join(QLatin1Char('\n')));
                             }});
        connect(te, &QPlainTextEdit::textChanged, this, [this, te, key]() {
            std::vector<std::string> values;
            for (const auto &line : te->toPlainText().split(QLatin1Char('\n'))) {
                const auto entry = line.trimmed();
                if (!entry.isEmpty())
                    values.push_back(entry.toStdString());
            }
            applyValue(te, key, values);
        });
        return te;
    }
    }

    return new QLabel(QStringLiteral("(unsupported parameter type)"), this);
}
