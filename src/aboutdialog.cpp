/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "config.h"

#include <QFontDatabase>

const QString aboutDlgCopyInfo = QStringLiteral(
        "<html>"
        "(c) 2016-2021 Matthias Klumpp"
        "<p>Developed at the Draguhn Group at Heidelberg University, Germany</p>"
        "<p>Syntalos is free software: you can redistribute it and/or modify "
        "it under the terms of the GNU General Public License as published by "
        "the Free Software Foundation, either version 3 of the License, or "
        "(at your option) any later version.</p>"
        "<p>Syntalos is distributed in the hope that it will be useful, "
        "but WITHOUT ANY WARRANTY; without even the implied warranty of "
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
        "GNU General Public License for more details.</p>"
        "<p>Icons are based on the Breeze Iconset by the <a href=\"https://kde.org/\">KDE Community</a> [LGPLv3+]</p>"
        "<h3>Modules:</h3>");

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    const auto syVersion = QStringLiteral("v%1").arg(QCoreApplication::applicationVersion());
    const auto syVcs = QStringLiteral(SY_VCS_TAG).replace(syVersion + QStringLiteral("-"), "");

    setWindowTitle(QStringLiteral("About Syntalos"));
    ui->asciiArtLabel->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    if (syVcs.isEmpty())
        ui->versionLabel->setText(syVersion);
    else
        ui->versionLabel->setText(QStringLiteral("%1 (%2)").arg(syVersion, syVcs));

    auto rect = geometry();
    rect.setWidth(ui->asciiArtLabel->width() + 10);
    setGeometry(rect);

    if (parent != nullptr)
        move(parent->geometry().center() - geometry().center());

    auto palette = ui->licenseTextBrowser->palette();
    palette.setColor(QPalette::Normal, QPalette::Base, this->palette().color(QPalette::Normal, QPalette::Window));
    ui->licenseTextBrowser->setPalette(palette);
    m_licenseText = aboutDlgCopyInfo;
    ui->licenseTextBrowser->setText(m_licenseText);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::addModuleLicense(const QString &modName, const QString &license)
{
    if (license.isEmpty())
        return;

    m_licenseText.append(QStringLiteral("<p><b>%2:</b><br/>%3")
                                        .arg(modName)
                                        .arg(license));
    ui->licenseTextBrowser->setText(m_licenseText);
}
