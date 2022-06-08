/*
 * Copyright (C) 2019-2022 Matthias Klumpp <matthias@tenstral.net>
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

#include "sysinfodialog.h"
#include "ui_sysinfodialog.h"

#include <QIcon>
#include <QClipboard>

SysInfoDialog::SysInfoDialog(SysInfo *sysInfo, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SysInfoDialog)
{
    ui->setupUi(this);
    ui->iconOS->setPixmap(QIcon::fromTheme("preferences-system-linux").pixmap(24, 24));
    ui->iconCPU->setPixmap(QIcon::fromTheme("cpu").pixmap(24, 24));
    ui->iconSoftware->setPixmap(QPixmap(":/module/generic").scaled(24, 24));

    m_okTextPal = ui->valOSName->palette();
    m_hintTextPal = ui->valOSName->palette();
    m_warnTextPal = ui->valOSName->palette();

    // for errors / things which are issues
    m_warnTextPal.setColor(QPalette::WindowText, qRgb(218, 68, 83));

    // for things which may be issues, depending on the setup
    m_hintTextPal.setColor(QPalette::WindowText, qRgb(244, 119, 80));

    // OS Information
    ui->valOSName->setText(sysInfo->prettyOSName());
    ui->valKernel->setText(sysInfo->kernelInfo());
    setLabelTextStyle(sysInfo->checkKernel(), ui->valKernel);
    ui->valInitSystem->setText(sysInfo->initName());
    setLabelTextStyle(sysInfo->checkInitSystem(), ui->valInitSystem);
    ui->valUsbFsMemory->setText(QStringLiteral("%1 MB").arg(sysInfo->usbFsMemoryMb()));
    setLabelTextStyle(sysInfo->checkUsbFsMemory(), ui->valUsbFsMemory);

    ui->valMaxRealtimePriority->setText(QString::number(sysInfo->rtkitMaxRealtimePriority()));
    setLabelTextStyle(sysInfo->checkRtkitMaxRealtimePriority(), ui->valMaxRealtimePriority);
    ui->valMinNiceLevel->setText(QString::number(sysInfo->rtkitMinNiceLevel()));
    setLabelTextStyle(sysInfo->checkRtkitMinNiceLevel(), ui->valMinNiceLevel);
    ui->valMaxRTTimeUsec->setText(QStringLiteral("%1 µs").arg(sysInfo->rtkitMaxRTTimeUsec()));
    setLabelTextStyle(sysInfo->checkRtkitMaxRTTimeUsec(), ui->valMaxRTTimeUsec);

    // Hardware Information
    ui->valHWArch->setText(sysInfo->currentArchitecture());
    ui->valCPU0ModelName->setText(sysInfo->cpu0ModelName());
    ui->valLogicalCores->setText(QString::number(sysInfo->cpuCount()));
    ui->valPhysicalCores->setText(QString::number(sysInfo->cpuPhysicalCoreCount()));

    ui->valClocksourceCurrent->setText(sysInfo->currentClocksource());
    setLabelTextStyle(sysInfo->checkClocksource(), ui->valClocksourceCurrent);
    ui->valClocksourceAvailable->setText(sysInfo->availableClocksources());
    ui->valConstantTSC->setText(sysInfo->tscIsConstant()? QStringLiteral("yes") : QStringLiteral("no"));
    setLabelTextStyle(sysInfo->checkTSCConstant(), ui->valConstantTSC);

    ui->valAVX->setText(sysInfo->supportedAVXInstructions());
    setLabelTextStyle(sysInfo->checkAVXInstructions(), ui->valAVX);
    ui->valOpenGL->setText(sysInfo->glVersion());

    // Software Information
    ui->valSyntalos->setText(sysInfo->syntalosVersion());
    ui->valQt->setText(sysInfo->qtVersion());
    ui->valOpenCV->setText(sysInfo->openCVVersionString());
    ui->valEigen->setText(sysInfo->eigenVersionString());
    ui->valFFmpeg->setText(sysInfo->ffmpegVersionString());
    ui->valPythonAPI->setText(sysInfo->pythonApiVersion());
    if (sysInfo->inFlatpakSandbox())
        ui->valSandboxInfo->setText(QStringLiteral("Flatpak; Runtime: %1 %2")
                                    .arg(sysInfo->runtimeName(), sysInfo->runtimeVersion()));
    else
        ui->valSandboxInfo->setText(QStringLiteral("None detected"));
}

SysInfoDialog::~SysInfoDialog()
{
    delete ui;
}

void SysInfoDialog::on_btnClipboardCopy_clicked()
{
    auto infoText = QStringLiteral("Syntalos %1 | %2 (Kernel: %3)\n")
                        .arg(ui->valSyntalos->text(),
                             ui->valOSName->text(),
                             ui->valKernel->text());
    infoText += QStringLiteral("Software: Qt %1 | OpenCV %2 | FFMpeg %3 | Python %4\n")
                        .arg(ui->valQt->text(),
                             ui->valOpenCV->text(),
                             ui->valFFmpeg->text(),
                             ui->valPythonAPI->text());
    infoText += QStringLiteral("Sandbox: %1\n").arg(ui->valSandboxInfo->text());
    infoText += QStringLiteral("Architecture: %1 | CPU: %2 | OpenGL: %3\n")
                        .arg(ui->valHWArch->text(),
                             ui->valCPU0ModelName->text(),
                             ui->valOpenGL->text());
    QGuiApplication::clipboard()->setText(infoText);
}

void SysInfoDialog::setLabelTextStyle(SysInfoCheckResult checkResult, QLabel *label)
{
    if (checkResult == SysInfoCheckResult::OK) {
        label->setPalette(m_okTextPal);
        return;
    }
    if (checkResult == SysInfoCheckResult::SUSPICIOUS) {
        label->setPalette(m_hintTextPal);
        return;
    }
    if (checkResult == SysInfoCheckResult::ISSUE) {
        label->setPalette(m_warnTextPal);
        return;
    }
}
