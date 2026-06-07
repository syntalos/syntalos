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

#include "whatsnewdialog.h"
#include "ui_whatsnewdialog.h"

#include <QDesktopServices>
#include <QFile>
#include <QIcon>
#include <QPushButton>
#include <QUrl>
#include <QVariantHash>
#include <QVariantList>

#include "utils/misc.h"
#include "utils/tomlutils.h"

constexpr const char *kBodyCss =
    "<style>"
    "h1, h2, h3 { margin-top: 0.6em; margin-bottom: 0.2em; }"
    "p { margin: 0.3em 0; }"
    "a { text-decoration: none; }"
    "</style>";

static QString changelogAnchorUrl(const QString &version)
{
    QString anchor;
    for (const QChar &c : version) {
        if (c.isDigit())
            anchor.append(c);
    }

    return QStringLiteral("https://syntalos.org/get/changes/#version-%1").arg(anchor);
}

static QIcon themeIconWithFallbacks(const QString &name, const QStringList &fallbacks)
{
    if (!name.isEmpty() && QIcon::hasThemeIcon(name))
        return QIcon::fromTheme(name);
    for (const auto &fb : fallbacks) {
        if (QIcon::hasThemeIcon(fb))
            return QIcon::fromTheme(fb);
    }

    return QIcon::fromTheme(name);
}

static QVariantHash findEntry(const QString &version)
{
    QFile rc(QStringLiteral(":/texts/whatsnew.toml"));
    if (!rc.open(QIODevice::ReadOnly))
        return {};

    QString parseError;
    const auto data = parseTomlData(rc.readAll(), parseError);
    if (!parseError.isEmpty())
        return {};

    const auto entries = data.value("entry", QVariantList()).toList();
    for (const auto &v : entries) {
        const auto entry = v.toHash();
        if (entry.value("version").toString() == version)
            return entry;
    }

    return {};
}

WhatsNewDialog::WhatsNewDialog(const QString &previousVersion, const QString &currentVersion, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::WhatsNewDialog)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("Syntalos has been updated"));

    const auto entry = findEntry(currentVersion);

    // Banner icon (theme-driven, with safe fallbacks)
    const QIcon bannerIcon = themeIconWithFallbacks(
        QStringLiteral("package-new"),
        {QStringLiteral("package-x-generic"), QStringLiteral("emblem-favorite")});
    ui->iconLabel->setPixmap(bannerIcon.pixmap(48, 48));
    setWindowIcon(bannerIcon);

    // Title
    const auto title = entry.value("title").toString().trimmed();
    if (title.isEmpty())
        ui->titleLabel->setText(QStringLiteral("Welcome to Syntalos %1!").arg(currentVersion));
    else
        ui->titleLabel->setText(title);

    // Released versions have no VCS suffix, so a difference between the plain and the full version string
    // means we are running a development build.
    const QString fullVersion = syntalosVersionFull();
    const bool isDevBuild = fullVersion != currentVersion;
    QString subtitle = QStringLiteral("Updated from v%1 ➜ v%2").arg(previousVersion, currentVersion);
    if (isDevBuild)
        subtitle += QStringLiteral(" — development build %1").arg(fullVersion);
    ui->subtitleLabel->setText(subtitle);
    auto subPalette = ui->subtitleLabel->palette();
    subPalette.setColor(QPalette::WindowText, palette().color(QPalette::Disabled, QPalette::WindowText));
    ui->subtitleLabel->setPalette(subPalette);

    // Body
    auto bodyPalette = ui->bodyBrowser->palette();
    bodyPalette.setColor(QPalette::Normal, QPalette::Base, this->palette().color(QPalette::Normal, QPalette::Window));
    ui->bodyBrowser->setPalette(bodyPalette);

    const auto bodyHtml = entry.value("body").toString().trimmed();
    QString finalBody = QString::fromUtf8(kBodyCss);
    if (isDevBuild) {
        finalBody += QStringLiteral("<p><b>This is a development version that contains unreleased changes!</b></p>");
    }
    if (bodyHtml.isEmpty()) {
        finalBody += QStringLiteral(
            "<p>This release contains fixes and improvements.</p>"
            "<p>See the full changelog online for the list of changes."
            "You can view the changes by clicking the <em>View all changes</em> button below.</p>");
    } else {
        finalBody += bodyHtml;
    }
    ui->bodyBrowser->setHtml(finalBody);

    const QString linkUrl = changelogAnchorUrl(currentVersion);
    const QIcon linkIcon = themeIconWithFallbacks(
        QStringLiteral("globe"),
        {QStringLiteral("applications-internet"), QStringLiteral("internet-web-browser")});

    auto linkButton = ui->buttonBox->addButton(QStringLiteral("View all changes"), QDialogButtonBox::ActionRole);
    linkButton->setIcon(linkIcon);
    linkButton->setDefault(true);
    linkButton->setAutoDefault(true);
    connect(linkButton, &QPushButton::clicked, this, [linkUrl]() {
        QDesktopServices::openUrl(QUrl(linkUrl));
    });
}

WhatsNewDialog::~WhatsNewDialog()
{
    delete ui;
}
