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
#include <QString>

class AcquisitionBoard;

class QLabel;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QThread;

/**
 * Modal dialog that runs @c AcquisitionBoard::measureImpedances on a worker
 * thread, shows progress, and presents the per-channel results when done.
 *
 * The dialog is parameterized by a non-owning @c AcquisitionBoard pointer.
 * Open / start / cancel / close transitions are handled internally.
 */
class OeAcqImpedanceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OeAcqImpedanceDialog(AcquisitionBoard *board, QWidget *parent = nullptr);
    ~OeAcqImpedanceDialog() override;

private slots:
    void onStartClicked();
    void onCancelClicked();
    void onSaveAsClicked();
    void onProgress(double fraction, const QString &status);
    void onScanFinished();

private:
    void buildUi();
    void populateResults();
    void setControlsRunning(bool running);
    QByteArray serializeImpedancesXml() const;

    AcquisitionBoard *m_board = nullptr;
    QThread *m_worker = nullptr;
    bool m_haveResults = false;

    QLabel *m_statusLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_saveAsButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QTableWidget *m_resultsTable = nullptr;
};
