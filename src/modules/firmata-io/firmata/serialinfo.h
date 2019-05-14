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

#ifndef QFIRMATA_BACKEND_SERIALINFO_H
#define QFIRMATA_BACKEND_SERIALINFO_H

#include <QObject>
#include <QAbstractListModel>

class SerialPortList : public QAbstractListModel {
public:
    enum SerialPortRoles {
        NameRole = Qt::UserRole+1,
        SystemLocationRole,
        DescriptionRole,
        ProductIdRole,
        VendorIdrole,
        ManufacturerRole,
        SerialNumberRole,
    };

    SerialPortList(QObject *parent=nullptr);

    int rowCount(const QModelIndex &parent=QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;

    QHash<int, QByteArray> roleNames() const;

public slots:
    void refresh();

private:
    struct Private;
    Private *d;
};


#endif
