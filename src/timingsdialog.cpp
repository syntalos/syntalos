/*
 * Copyright (C) 2019-2020 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "timingsdialog.h"
#include "ui_timingsdialog.h"

using namespace Syntalos;

TimingDisplayWidget::TimingDisplayWidget(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    m_lblTitle = new QLabel(this);
    auto font = m_lblTitle->font();
    font.setBold(true);
    font.setPointSize(10);
    m_lblTitle->setFont(font);
    m_lblTitle->setText(title);

    m_lblStrategies = new QLabel(this);
    m_lblTolerance = new QLabel(this);
    m_lblInterval = new QLabel(this);
    m_lblOffset = new QLabel(this);
    m_lblInfo = new QLabel(this);

    auto layout = new QGridLayout;
    layout->setMargin(0);

    layout->addWidget(m_lblTitle, 0, 0, 1, 2);

    layout->addWidget(new QLabel(QStringLiteral("Check Interval"), this), 1, 0);
    layout->addWidget(m_lblInterval, 1, 1);

    layout->addWidget(new QLabel(QStringLiteral("Tolerance"), this), 2, 0);
    layout->addWidget(m_lblTolerance, 2, 1);

    layout->addWidget(new QLabel(QStringLiteral("Offset"), this), 3, 0);
    layout->addWidget(m_lblOffset, 3, 1);

    layout->addWidget(new QLabel(QStringLiteral("Strategies"), this), 4, 0);
    layout->addWidget(m_lblStrategies, 4, 1);

    layout->addWidget(m_lblInfo, 5, 0, 1, 2);

    setLayout(layout);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
}

void TimingDisplayWidget::setStrategies(const TimeSyncStrategies &strategies)
{
    m_lblStrategies->setText(timeSyncStrategiesToHString(strategies));
}

void TimingDisplayWidget::setCheckInterval(const std::chrono::microseconds &interval)
{
    if (interval.count() <= 0)
        m_lblInterval->setText(QStringLiteral("continuous"));
    else
        m_lblInterval->setText(QStringLiteral("%1 ms").arg(interval.count() / 1000.0));
}

void TimingDisplayWidget::setTolerance(const std::chrono::microseconds &tolerance)
{
    m_lblTolerance->setText(QStringLiteral("%1 ms").arg(tolerance.count() / 1000.0));
}

void TimingDisplayWidget::setCurrentOffset(const std::chrono::microseconds &offset)
{
    double offsetCount = offset.count();
    m_lblOffset->setText(QStringLiteral("%1 ms").arg(offsetCount / 1000.0));
}

TimingsDialog::TimingsDialog(QWidget *parent)
    : QDialog(parent),
      ui(new Ui::TimingsDialog)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("System Timing & Latency Information"));
}

TimingsDialog::~TimingsDialog()
{
    delete ui;
}

void TimingsDialog::onSynchronizerDetailsChanged(const QString &id, const TimeSyncStrategies &strategies, const std::chrono::microseconds tolerance, const std::chrono::microseconds checkInterval)
{
    Q_UNUSED(id)
    auto mod = ::qobject_cast<AbstractModule*>(sender());
    if (mod == nullptr)
        return;
    auto tdisp = m_tdispMap.value(mod);
    if (tdisp == nullptr) {
        tdisp = new TimingDisplayWidget(mod->name(), this);
        if ((m_tdispMap.count() % 2) == 0)
            ui->layoutLeftPanel->insertWidget(ui->layoutLeftPanel->count() - 1, tdisp);
        else
            ui->layoutRightPanel->insertWidget(ui->layoutRightPanel->count() - 1, tdisp);
        m_tdispMap[mod] = tdisp;
    }
    tdisp->setStrategies(strategies);
    tdisp->setCheckInterval(checkInterval);
    tdisp->setTolerance(tolerance);
}

void TimingsDialog::onSynchronizerOffsetChanged(const QString &id, const std::chrono::microseconds currentOffset)
{
    Q_UNUSED(id)
    auto mod = ::qobject_cast<AbstractModule*>(sender());
    if (mod == nullptr)
        return;
    auto tdisp = m_tdispMap.value(mod);
    if (tdisp == nullptr)
        return;
    tdisp->setCurrentOffset(currentOffset);
}

void TimingsDialog::clear()
{
    foreach (auto w, m_tdispMap.values())
        delete w;
    m_tdispMap.clear();
}


