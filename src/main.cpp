/*
 * Copyright (C) 2012-2016 Matthias Klumpp <matthias@tenstral.net>
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

#include "mainwindow.h"
#include <QApplication>
#include <QSplashScreen>
#include <QTimer>
#include <QTime>
#include <QDebug>
#include <KDBusService>

static QString const splashMessages[] =
    { "Loading MazeAmaze...",
      "Finding Nemo...",
      "Calculating age of the universe...",
      "Obtaining a philosopher's stone...",
      "Contemplating my own existence...",
      "Connecting to SkyNet...",
      "Researching how to wipe out humanity...",
      "Counting mice...",
      "Counting rats...",
      "Petting mice...",
      "Writing thesis...",
      "Done.",
      "Waiting for you...",
      "Reading your emails...",
      "Solving Riemann hypothesis...",
      "Doing research...",
      "Creating a universe...",
      "Finishing conversation with mice...",
      "Connecting to Intan device...",
      "Annoying my supervisor...",
      "Determining the meaning of life...",
      "Treating mice with Chemical X...",
      "Running away...",
      "Getting bored...",
      "Readying Infinite Improbability Drive...",
      "Getting excited...",
      "Counting stars...",
      "Dealing with the devil...",
    };
static int const splashMessagesCount = sizeof(splashMessages) / sizeof(splashMessages[0]);


static void splashUpdateMessage(QSplashScreen& splash, const QString& message)
{
    splash.showMessage(message, Qt::AlignBottom | Qt::AlignRight, QColor("#ffe199"));
}


int main(int argc, char *argv[])
{
    // set random seed
    qsrand(QTime::currentTime().msec());

    QApplication app(argc, argv);
    app.setApplicationName("MazeAmaze");
    app.setOrganizationDomain("uni-heidelberg.de");
    app.setApplicationVersion("1.0");

    // ensure we only ever run one instance of the application
    KDBusService service(KDBusService::Unique);

    QPixmap pixmap(":/images/splash");
    QSplashScreen splash(pixmap);
    QFont splashFont;
    splashFont.setFamily("Noto");
    splashFont.setBold(true);
    splash.setFont(splashFont);

    splash.show();
    app.processEvents();

    auto message = splashMessages[qrand() % (splashMessagesCount - 1)];
    splashUpdateMessage(splash, message);
    app.processEvents();

    QTimer *timer = new QTimer;
    timer->setSingleShot(true);

    bool done = false;
    QTimer::connect(timer, &QTimer::timeout, [&]() {
        done = true;
        timer->deleteLater();

        if (!splash.isVisible())
            return;
        auto message = splashMessages[qrand() % (splashMessagesCount - 1)];
        splashUpdateMessage(splash, message);
        app.processEvents();
    });
    timer->start(3000);

    MainWindow w;
    w.show();
    splash.finish(&w);

    return app.exec();
}
