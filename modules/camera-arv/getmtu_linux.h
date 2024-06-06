/*
    QArv, a Qt interface to aravis.
    Copyright (C) 2012, 2013 Jure Varlec <jure.varlec@ad-vega.si>
                             Andrej Lajovic <andrej.lajovic@ad-vega.si>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GETMTU_LINUX_H
#define GETMTU_LINUX_H

namespace QArv
{

#include <QDebug>
#include <QString>
#include <cstring>

extern "C" {
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
}

static int getMTU(QString ifname)
{
    struct ifreq req;
    auto bytes = ifname.toLatin1();
    strcpy(req.ifr_name, bytes.constData());
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        qDebug().noquote() << "mtu: Socket creation error.";
        return 0;
    }
    ioctl(sock, SIOCGIFMTU, &req);
    close(sock);
    return req.ifr_mtu;
}

} // namespace QArv

#endif
