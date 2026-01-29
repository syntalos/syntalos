/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "simpleterminal.h"

#include <QHBoxLayout>
#include <QToolButton>
#include <QPushButton>
#include <QLineEdit>
#include <QStyle>
#include <QIcon>
#include <QApplication>
#include <qtermwidget6/qtermwidget.h>

using namespace Syntalos;

SimpleTerminal::SimpleTerminal(QWidget *parent)
    : QWidget(parent)
{
    setWindowIcon(QIcon(":/icons/terminal"));
    setWindowTitle(QStringLiteral("Syntalos Terminal"));

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto buttonBar = new QWidget(this);
    buttonBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto buttonLayout = new QHBoxLayout(buttonBar);
    buttonLayout->setContentsMargins(4, 4, 4, 4);
    buttonLayout->setSpacing(4);

    // Copy button
    m_copyBtn = new QToolButton(buttonBar);
    m_copyBtn->setIcon(QIcon::fromTheme("edit-copy", QApplication::style()->standardIcon(QStyle::SP_DialogYesButton)));
    m_copyBtn->setToolTip(tr("Copy selected text to clipboard"));
    m_copyBtn->setText(tr("Copy"));
    m_copyBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_copyBtn->setAutoRaise(true);
    connect(m_copyBtn, &QToolButton::clicked, this, &SimpleTerminal::onCopyClicked);

    // Paste button
    m_pasteBtn = new QToolButton(buttonBar);
    m_pasteBtn->setIcon(
        QIcon::fromTheme("edit-paste", QApplication::style()->standardIcon(QStyle::SP_DialogApplyButton)));
    m_pasteBtn->setToolTip(tr("Paste clipboard content to terminal"));
    m_pasteBtn->setText(tr("Paste"));
    m_pasteBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_pasteBtn->setAutoRaise(true);
    connect(m_pasteBtn, &QToolButton::clicked, this, &SimpleTerminal::onPasteClicked);

    // Clear button
    auto clearBtn = new QToolButton(buttonBar);
    clearBtn->setIcon(
        QIcon::fromTheme("edit-clear", QApplication::style()->standardIcon(QStyle::SP_DialogResetButton)));
    clearBtn->setToolTip(tr("Clear terminal screen"));
    clearBtn->setText(tr("Clear"));
    clearBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    clearBtn->setAutoRaise(true);
    connect(clearBtn, &QToolButton::clicked, this, &SimpleTerminal::onClearClicked);

    // Search button
    m_searchBtn = new QToolButton(buttonBar);
    m_searchBtn->setIcon(
        QIcon::fromTheme("edit-find", QApplication::style()->standardIcon(QStyle::SP_FileDialogDetailedView)));
    m_searchBtn->setToolTip(tr("Search in terminal"));
    m_searchBtn->setText(tr("Search"));
    m_searchBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_searchBtn->setAutoRaise(true);
    connect(m_searchBtn, &QToolButton::clicked, this, &SimpleTerminal::onSearchToggled);

    buttonLayout->addWidget(m_copyBtn);
    buttonLayout->addWidget(m_pasteBtn);
    buttonLayout->addWidget(clearBtn);
    buttonLayout->addWidget(m_searchBtn);
    buttonLayout->addStretch();

    mainLayout->addWidget(buttonBar, 0); // 0 stretch factor - minimum size only

    // Terminal widget
    m_termWidget = new QTermWidget(0, this);
    m_termWidget->setColorScheme("Linux");
    m_termWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(m_termWidget, &QTermWidget::finished, this, &SimpleTerminal::finished);
    connect(m_termWidget, &QTermWidget::activity, this, &SimpleTerminal::activity);

    mainLayout->addWidget(m_termWidget, 1); // 1 stretch factor - take all remaining space

    setLayout(mainLayout);
    resize(800, 400);
}

SimpleTerminal::~SimpleTerminal() {}

QTermWidget *SimpleTerminal::termWidget() const
{
    return m_termWidget;
}

void SimpleTerminal::sendText(const QString &text)
{
    m_termWidget->sendText(text);
}

void SimpleTerminal::setWorkingDirectory(const QString &dir)
{
    m_termWidget->setWorkingDirectory(dir);
}

QString SimpleTerminal::workingDirectory() const
{
    return m_termWidget->workingDirectory();
}

void SimpleTerminal::setShellProgram(const QString &shellPath)
{
    m_termWidget->setShellProgram(shellPath);
}

void SimpleTerminal::startShell()
{
    m_termWidget->startShellProgram();
}

void SimpleTerminal::clear()
{
    m_termWidget->clear();
}

void SimpleTerminal::onCopyClicked()
{
    m_termWidget->copyClipboard();
}

void SimpleTerminal::onPasteClicked()
{
    m_termWidget->pasteClipboard();
}

void SimpleTerminal::onClearClicked()
{
    m_termWidget->clear();
}

void SimpleTerminal::onSearchToggled(bool checked)
{
    m_termWidget->toggleShowSearchBar();
}
