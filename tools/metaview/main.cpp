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
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <QCoreApplication>
#include <QCommandLineParser>
#include <iostream>

#include "readtsync.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("syntalos-metaview");
    QCoreApplication::setApplicationVersion(PROJECT_VERSION);


    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Syntalos MetaView\n\nRead and display metadata from (binary) files."));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption tsyncOption(QStringLiteral("tsync"),
                                   QStringLiteral("Read data from a time-sync (.tsync) file"),
                                   QStringLiteral("file"));
    parser.addOption(tsyncOption);

    parser.process(a);

    QString tsyncFile = parser.value(tsyncOption);
    if (!tsyncFile.isEmpty())
        return displayTSyncMetadata(tsyncFile);
    else {
        std::cout << parser.helpText().toStdString() << std::endl;
        return 0;
    }

    return a.exec();
}
