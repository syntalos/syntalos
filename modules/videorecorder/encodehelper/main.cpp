/*
 * Copyright (C) 2020-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "config.h"
#include "encodewindow.h"

#include <QApplication>
#include <QMessageBox>

#include "appstyle.h"
#include "datactl/vips8-q.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Syntalos.EncodeHelper");
    app.setOrganizationName("DraguhnLab");
    app.setOrganizationDomain("draguhnlab.com");
    app.setApplicationVersion(PROJECT_VERSION);

    // initailize VIPS
    if (VIPS_INIT(argv[0])) {
        QMessageBox::critical(nullptr, "Critical Error", "Failed to initialize: Unable to start VIPS");
        vips_error_exit(NULL);
    }

    EncodeWindow w;

    // set Syntalos default style
    setDefaultStyle();

    w.show();
    return app.exec();
}
