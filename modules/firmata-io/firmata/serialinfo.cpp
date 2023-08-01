// QFirmata - a Firmata library for QML
//
// Copyright 2016 - Calle Laakkonen
//
// QFirmata is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// QFirmata is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Foobar.  If not, see <http://www.gnu.org/licenses/>.

#include "serialinfo.h"

#include <QSerialPortInfo>

struct SerialPortList::Private {
    QList<QSerialPortInfo> ports;
};

SerialPortList::SerialPortList(QObject *parent)
    : QAbstractListModel(parent),
      d(new Private)
{
    refresh();
}

int SerialPortList::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return d->ports.size();
}

QVariant SerialPortList::data(const QModelIndex &index, int role) const
{
    if (index.isValid() && index.row() >= 0 && index.row() < d->ports.size()) {
        const QSerialPortInfo &p = d->ports[index.row()];
        switch (role) {
        case Qt::DisplayRole:
        case NameRole:
            return p.portName();
        case SystemLocationRole:
            return p.systemLocation();
        case DescriptionRole:
            return p.description();
        case ProductIdRole:
            return p.productIdentifier();
        case VendorIdrole:
            return p.vendorIdentifier();
        case ManufacturerRole:
            return p.manufacturer();
        case SerialNumberRole:
            return p.serialNumber();
        }
    }

    return QVariant();
}

QHash<int, QByteArray> SerialPortList::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Qt::DisplayRole] = "display";
    roles[NameRole] = "name";
    roles[SystemLocationRole] = "systemLocation";
    roles[DescriptionRole] = "description";
    roles[ProductIdRole] = "productId";
    roles[VendorIdrole] = "vendorId";
    roles[ManufacturerRole] = "manufacturer";
    roles[SerialNumberRole] = "serialNumber";
    return roles;
}

void SerialPortList::refresh()
{
    beginResetModel();
    d->ports = QSerialPortInfo::availablePorts();
    endResetModel();
}
