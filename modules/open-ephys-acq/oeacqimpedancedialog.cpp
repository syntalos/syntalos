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

#include "oeacqimpedancedialog.h"

#include "devices/AcquisitionBoard.h"
#include "devices/Headstage.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QVBoxLayout>
#include <QXmlStreamWriter>

namespace
{
/** Format an impedance magnitude (Ohms) for tabular display. */
QString fmtMagnitude(double ohms)
{
    if (ohms >= 1.0e6)
        return QStringLiteral("%1 MΩ").arg(ohms / 1.0e6, 0, 'f', 2);
    if (ohms >= 1.0e3)
        return QStringLiteral("%1 kΩ").arg(ohms / 1.0e3, 0, 'f', 1);
    return QStringLiteral("%1 Ω").arg(ohms, 0, 'f', 1);
}
} // namespace

/* ------------------------------------------------------------------ */
/* Worker that runs the synchronous impedance scan on its own thread. */
/* ------------------------------------------------------------------ */

namespace
{
class ImpedanceWorker : public QObject
{
    Q_OBJECT
public:
    explicit ImpedanceWorker(AcquisitionBoard *board)
        : m_board(board)
    {
    }

public slots:
    void run()
    {
        if (m_board)
            m_board->measureImpedances();
        emit finished();
    }

signals:
    void finished();

private:
    AcquisitionBoard *m_board;
};
} // namespace

#include "oeacqimpedancedialog.moc"

/* ------------------------------------------------------------------ */
/* Dialog                                                             */
/* ------------------------------------------------------------------ */

OeAcqImpedanceDialog::OeAcqImpedanceDialog(AcquisitionBoard *board, QWidget *parent)
    : QDialog(parent),
      m_board(board)
{
    setWindowTitle(QStringLiteral("Measure Electrode Impedances"));
    setModal(true);
    resize(560, 480);
    buildUi();
    setControlsRunning(false);
}

OeAcqImpedanceDialog::~OeAcqImpedanceDialog()
{
    if (m_worker && m_worker->isRunning()) {
        if (m_board)
            m_board->requestImpedanceStop();
        m_worker->quit();
        m_worker->wait(2000);
    }
}

void OeAcqImpedanceDialog::buildUi()
{
    auto root = new QVBoxLayout(this);

    m_statusLabel = new QLabel(
        QStringLiteral(
            "Press <b>Start scan</b> to measure impedances on all connected channels.\n"
            "The board will be temporarily reconfigured (30 kHz sample rate, "
            "default bandwidths, DSP off) and restored afterwards."),
        this);
    m_statusLabel->setWordWrap(true);
    root->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 1000);
    m_progressBar->setValue(0);
    root->addWidget(m_progressBar);

    m_resultsTable = new QTableWidget(this);
    m_resultsTable->setColumnCount(4);
    m_resultsTable->setHorizontalHeaderLabels(
        {QStringLiteral("Stream"),
         QStringLiteral("Channel"),
         QStringLiteral("Magnitude"),
         QStringLiteral("Phase (°)")});
    m_resultsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_resultsTable->verticalHeader()->setVisible(false);
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    root->addWidget(m_resultsTable, /*stretch*/ 1);

    auto btnRow = new QHBoxLayout;
    btnRow->addStretch(1);

    m_startButton = new QPushButton("Start scan", this);
    m_startButton->setDefault(true);
    btnRow->addWidget(m_startButton);

    m_cancelButton = new QPushButton("Cancel", this);
    btnRow->addWidget(m_cancelButton);

    m_saveAsButton = new QPushButton("Save as…", this);
    m_saveAsButton->setEnabled(false);
    btnRow->addWidget(m_saveAsButton);

    m_closeButton = new QPushButton("Close", this);
    btnRow->addWidget(m_closeButton);

    root->addLayout(btnRow);

    connect(m_startButton, &QPushButton::clicked, this, &OeAcqImpedanceDialog::onStartClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &OeAcqImpedanceDialog::onCancelClicked);
    connect(m_saveAsButton, &QPushButton::clicked, this, &OeAcqImpedanceDialog::onSaveAsClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void OeAcqImpedanceDialog::setControlsRunning(bool running)
{
    m_startButton->setEnabled(!running);
    m_cancelButton->setEnabled(running);
    m_closeButton->setEnabled(!running);
    m_saveAsButton->setEnabled(!running && m_haveResults);
}

void OeAcqImpedanceDialog::onStartClicked()
{
    if (!m_board) {
        m_statusLabel->setText(QStringLiteral("No acquisition board available."));
        return;
    }

    // Refuse to scan when no headstages are attached — there's nothing to
    // measure, and the underlying meter assumes at least one connected
    // stream when allocating its working buffers.
    bool anyConnected = false;
    for (const auto hs : m_board->getHeadstages()) {
        if (hs && hs->isConnected()) {
            anyConnected = true;
            break;
        }
    }
    if (!anyConnected) {
        m_statusLabel->setText(QStringLiteral(
            "<b>No headstages connected.</b><br/>"
            "Plug in at least one headstage and use <i>Rescan headstages</i> "
            "in the settings dialog before measuring impedances."));
        return;
    }

    m_resultsTable->setRowCount(0);
    m_progressBar->setValue(0);
    m_haveResults = false;
    m_statusLabel->setText(QStringLiteral("Scanning… please wait."));

    // Hook the meter's progress callback up to a queued connection.
    m_board->setImpedanceProgressCallback([this](double frac, const std::string &status) {
        QMetaObject::invokeMethod(
            this,
            [this, frac, status]() {
                onProgress(frac, QString::fromStdString(status));
            },
            Qt::QueuedConnection);
    });

    auto worker = new ImpedanceWorker(m_board);
    m_worker = new QThread(this);
    worker->moveToThread(m_worker);
    connect(m_worker, &QThread::started, worker, &ImpedanceWorker::run);
    connect(worker, &ImpedanceWorker::finished, this, &OeAcqImpedanceDialog::onScanFinished);
    connect(worker, &ImpedanceWorker::finished, worker, &QObject::deleteLater);
    connect(worker, &ImpedanceWorker::finished, m_worker, &QThread::quit);
    connect(m_worker, &QThread::finished, m_worker, &QObject::deleteLater);

    setControlsRunning(true);
    m_worker->start();
}

void OeAcqImpedanceDialog::onCancelClicked()
{
    if (m_board)
        m_board->requestImpedanceStop();
    m_statusLabel->setText(QStringLiteral("Cancelling…"));
    m_cancelButton->setEnabled(false);
}

void OeAcqImpedanceDialog::onProgress(double fraction, const QString &status)
{
    m_progressBar->setValue(static_cast<int>(fraction * 1000.0 + 0.5));
    if (!status.isEmpty())
        m_statusLabel->setText(status);
}

void OeAcqImpedanceDialog::onScanFinished()
{
    // Detach the progress callback — the meter outlives the dialog.
    if (m_board)
        m_board->setImpedanceProgressCallback({});
    m_worker = nullptr;

    setControlsRunning(false);
    m_progressBar->setValue(1000);
    m_statusLabel->setText(QStringLiteral("Scan complete."));
    populateResults();
}

void OeAcqImpedanceDialog::populateResults()
{
    if (!m_board) {
        m_resultsTable->setRowCount(0);
        return;
    }

    const Impedances &imps = m_board->getImpedances();
    if (!imps.valid) {
        m_resultsTable->setRowCount(0);
        m_haveResults = false;
        m_saveAsButton->setEnabled(false);
        m_statusLabel->setText(QStringLiteral("Scan completed, but no impedance data was returned."));
        return;
    }

    m_haveResults = true;
    m_saveAsButton->setEnabled(true);
    const int n = static_cast<int>(imps.streams.size());
    m_resultsTable->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        m_resultsTable->setItem(i, 0, new QTableWidgetItem(QString::number(imps.streams[i])));
        m_resultsTable->setItem(i, 1, new QTableWidgetItem(QString::number(imps.channels[i])));
        m_resultsTable->setItem(i, 2, new QTableWidgetItem(fmtMagnitude(static_cast<double>(imps.magnitudes[i]))));
        m_resultsTable->setItem(
            i,
            3,
            new QTableWidgetItem(QString::number(static_cast<double>(imps.phases[i]), 'f', 1)));
    }
    m_resultsTable->resizeColumnsToContents();
}

void OeAcqImpedanceDialog::onSaveAsClicked()
{
    if (!m_board || !m_haveResults)
        return;

    const QString defaultName = QStringLiteral("impedances.xml");
    const QString path = QFileDialog::getSaveFileName(
        this,
        "Save Impedance Measurements",
        defaultName,
        "XML Files (*.xml);;All Files (*)");
    if (path.isEmpty())
        return;

    const QByteArray xml = serializeImpedancesXml();
    if (xml.isEmpty()) {
        QMessageBox::warning(this, "Save Impedance Measurements", "No impedance data is available to save.");
        return;
    }

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(xml) != xml.size() || !f.commit()) {
        QMessageBox::critical(
            this,
            "Save Impedance Measurements",
            QStringLiteral("Could not write impedance data to:\n%1\n\n%2").arg(path, f.errorString()));
        return;
    }

    m_statusLabel->setText(QStringLiteral("Saved impedances to <i>%1</i>.").arg(QFileInfo(path).fileName()));
}

QByteArray OeAcqImpedanceDialog::serializeImpedancesXml() const
{
    if (!m_board)
        return {};
    const auto &imps = m_board->getImpedances();
    if (!imps.valid)
        return {};

    QByteArray out;
    QXmlStreamWriter w(&out);
    w.setAutoFormatting(true);
    w.writeStartDocument();
    w.writeStartElement(QStringLiteral("IMPEDANCES"));

    int globalChannelNumber = -1;
    const auto headstages = m_board->getHeadstages();
    for (const auto hs : headstages) {
        if (!hs || !hs->isConnected())
            continue;
        w.writeStartElement(QStringLiteral("HEADSTAGE"));
        w.writeAttribute(QStringLiteral("name"), QString::fromStdString(hs->getStreamPrefix()));
        for (int ch = 0; ch < hs->getNumActiveChannels(); ch++) {
            globalChannelNumber++;
            w.writeStartElement(QStringLiteral("CHANNEL"));
            w.writeAttribute(QStringLiteral("name"), QString::fromStdString(hs->getChannelName(ch)));
            w.writeAttribute(QStringLiteral("number"), QString::number(globalChannelNumber));
            w.writeAttribute(QStringLiteral("magnitude"), QString::number(hs->getImpedanceMagnitude(ch)));
            w.writeAttribute(QStringLiteral("phase"), QString::number(hs->getImpedancePhase(ch)));
            w.writeEndElement(); // CHANNEL
        }
        w.writeEndElement(); // HEADSTAGE
    }

    w.writeEndElement(); // IMPEDANCES
    w.writeEndDocument();
    return out;
}
