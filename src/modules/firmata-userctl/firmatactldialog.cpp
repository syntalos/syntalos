/*
 * Copyright (C) 2010 Matthias Klumpp <matthias@tenstral.net>
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

#include "firmatactldialog.h"
#include "ui_firmatactldialog.h"

#include <QDebug>
#include <QSpinBox>
#include <QPushButton>
#include <QInputDialog>

FirmataOutputWidget::FirmataOutputWidget(std::shared_ptr<DataStream<FirmataControl> > fmCtlStream, bool analog, QWidget *parent)
    : QWidget(parent),
      m_isAnalog(analog),
      m_fmCtlStream(fmCtlStream)
{
    m_btnRemove = new QPushButton(this);
    m_btnRemove->setFlat(true);
    m_btnRemove->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    m_btnRemove->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
    connect(m_btnRemove, &QPushButton::clicked, [&]{
        deleteLater();
    });

    m_sbPinId = new QSpinBox(this);
    m_sbPinId->setPrefix(QStringLiteral("Pin: "));
    m_sbPinId->setRange(0, 255);

    m_btnSend = new QPushButton(this);
    m_btnSend->setText(analog? QStringLiteral("Send") : QStringLiteral("Off"));
    m_btnSend->setCheckable(!analog);

    m_sbValue = new QSpinBox(this);
    m_sbValue->setVisible(analog);
    m_sbValue->setPrefix(QStringLiteral("Value: "));
    m_sbValue->setRange(0, 65535);

    m_btnPulse = new QPushButton(this);
    m_btnPulse->setText(QStringLiteral("Pulse"));
    m_btnPulse->setVisible(!analog);

    const auto vLine1 = new QFrame(this);
    vLine1->setFrameShape(QFrame::VLine);
    const auto vLine2 = new QFrame(this);
    vLine2->setFrameShape(QFrame::VLine);

    auto layout = new QHBoxLayout;
    layout->setMargin(0);

    layout->addWidget(m_btnRemove);
    layout->addWidget(vLine1);

    layout->addWidget(m_sbPinId);
    layout->addWidget(vLine2);
    if (analog) {
        const auto vLine3 = new QFrame(this);
        vLine3->setFrameShape(QFrame::VLine);

        layout->addWidget(m_sbValue);
        layout->addWidget(vLine3);
    } else {
        layout->addWidget(m_btnPulse);
    }

    layout->addWidget(m_btnSend);

    setLayout(layout);
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);

    connect(m_btnSend, &QPushButton::clicked, [&]{
        FirmataControl ctl;
        ctl.pinId = m_sbPinId->value();

        if (m_isAnalog) {
            ctl.command = FirmataCommandKind::WRITE_ANALOG;
            ctl.value = m_sbValue->value();
        } else {
            ctl.command = FirmataCommandKind::WRITE_DIGITAL;
            ctl.value = m_btnSend->isChecked()? 1 : 0;
            m_btnSend->setText(m_btnSend->isChecked()? QStringLiteral("On") : QStringLiteral("Off"));
        }

        m_fmCtlStream->push(ctl);
    });

    connect(m_btnPulse, &QPushButton::clicked, [&]{
        m_btnSend->setChecked(false);
        m_btnSend->click();
        FirmataControl ctl;
        ctl.pinId = m_sbPinId->value();
        ctl.command = FirmataCommandKind::WRITE_DIGITAL_PULSE;
        ctl.value = 1;
        m_fmCtlStream->push(ctl);
    });

    // This shouldn't be here...
    FirmataControl newPinCtl;
    newPinCtl.pinId = m_sbPinId->value();
    newPinCtl.command = analog? FirmataCommandKind::NEW_ANA_PIN : FirmataCommandKind::NEW_DIG_PIN;
    m_fmCtlStream->push(newPinCtl);
}

bool FirmataOutputWidget::isAnalog() const
{
    return m_isAnalog;
}

void FirmataOutputWidget::setPinId(int pinId)
{
    m_sbPinId->setValue(pinId);
}

FirmataCtlDialog::FirmataCtlDialog(std::shared_ptr<DataStream<FirmataControl>> fmCtlStream, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FirmataCtlDialog),
    m_lastPinId(0),
    m_fmCtlStream(fmCtlStream)
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/icons/generic-view"));

}

FirmataCtlDialog::~FirmataCtlDialog()
{
    delete ui;
}

void FirmataCtlDialog::on_btnAddOutputControl_clicked()
{
    bool ok;
    auto item = QInputDialog::getItem(this,
                                      QStringLiteral("Select Data Type"),
                                      QStringLiteral("Data kind to add output control for:"),
                                      QStringList() << "Analog" << "Digital",
                                      0,
                                      false,
                                      &ok);
    if (!ok || item.isEmpty())
        return;

    auto w = new FirmataOutputWidget(m_fmCtlStream,
                                     item.startsWith(QStringLiteral("Analog")),
                                     ui->saOutputContents);
    qobject_cast<QVBoxLayout*>(ui->saOutputContents->layout())->insertWidget(0, w);

    w->setPinId(m_lastPinId);
    m_lastPinId++;
    if (m_lastPinId > 255)
        m_lastPinId = 0;
}

void FirmataCtlDialog::on_btnAddInputWatch_clicked()
{

}
