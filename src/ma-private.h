/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef MAPRIVATE_H
#define MAPRIVATE_H

#include <QString>

const QString aboutDlgAsciiArt = QStringLiteral(
                         "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@@@@@@@@         @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@@@@@@             @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@@@@                 @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@@@                   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@@@                   @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@@@@                 @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@@@@@@             @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@@@@@@@@         @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@@@ o @@@@@@@@ o @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@ =/ \\= @@@@ =/ \\= @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@@ / \" \\ @@@@ / \" \\ @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "@@@@@@@@@@@@@@ /     \\ @@ /     \\ @@@@@@@@@@@@@@@@@@@@@@@@@@hjw@@@@\n"
                         "@@@@@@@@@@@@@ /       \\ */       \\ @@@@@@@@@@@@@@@@@@@@@@@@@`97@@@@\n"
                         "@@@@@@@@@@@@ |         ||         | @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n"
                         "V#vvVVvv/y,~ \\__\\ /___/#v\\__\\ /__/~//y|#\\\\.vVvV/,y#||.,//vvVvVvV,,##\n"
                         "                 \\\\        //\n"
                         "                  \\\\      //\n"
                         "                   \\\\    //\n"
                         "                    \\\\  //\n"
                         "                     \\\\//\n"
                         "                      \\y\n"
                         "                     //\\\\\n"
                         "                     \\\\//\n"
                         "                      \\y\n"
                         "                     //\\\\\n"
                         "                     U  U");

const QString aboutDlgCopyInfo = QStringLiteral(
        "(c) 2016-2019 Matthias Klumpp\n\n"
        "Based on code by Intan Technologies, (c) 2013\n"
        "Firmata implementation (c) 2016 Calle Laakkonen\n\n"
        "MazeAmaze is free software: you can redistribute it and/or modify\n"
        "it under the terms of the GNU General Public License as published by\n"
        "the Free Software Foundation, either version 3 of the License, or\n"
        "(at your option) any later version.\n"
        "\n"
        "MazeAmaze is distributed in the hope that it will be useful,\n"
        "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
        "GNU General Public License for more details.");

const QString versionInfoText = QStringLiteral("v%1\nMake mice mazes great again!");

#endif // MAPRIVATE_H
