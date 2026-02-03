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

#pragma once

#include <QWidget>

class QTermWidget;
class QLineEdit;
class QPushButton;
class QToolButton;

namespace Syntalos
{

/**
 * @brief A simple integrated terminal emulator
 *
 * This widget provides a basic terminal emulator with copy, paste, and search
 * functionality. It can be used to run commands while still allowing user interaction.
 */
class SimpleTerminal : public QWidget
{
    Q_OBJECT

public:
    explicit SimpleTerminal(QWidget *parent = nullptr);
    ~SimpleTerminal() override;

    /**
     * @brief Get the underlying QTermWidget
     * @return Pointer to the terminal widget
     */
    [[nodiscard]] QTermWidget *termWidget() const;

    /**
     * @brief Send text to the terminal
     * @param text The text to send
     */
    void sendText(const QString &text);

    /**
     * @brief Set the working directory for the terminal
     * @param dir The directory path
     */
    void setWorkingDirectory(const QString &dir);

    /**
     * @brief Get the current working directory
     * @return The directory path
     */
    QString workingDirectory() const;

    /**
     * @brief Set the shell program to use
     * @param shellPath Path to the shell executable
     */
    void setShellProgram(const QString &shellPath);

    /**
     * @brief Start the shell program
     */
    void startShell();

    /**
     * @brief Clear the terminal screen
     */
    void clear();

signals:
    /**
     * @brief Emitted when the terminal process finishes
     */
    void finished();

    /**
     * @brief Emitted when the terminal receives activity
     */
    void activity();

    /**
     * @brief Emitted when the terminal window is closed
     */
    void windowClosed();

private slots:
    void onCopyClicked();
    void onPasteClicked();
    void onClearClicked();
    void onSearchToggled(bool checked);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QTermWidget *m_termWidget;
    QToolButton *m_copyBtn;
    QToolButton *m_pasteBtn;
    QToolButton *m_searchBtn;
};

} // namespace Syntalos
