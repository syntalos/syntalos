//------------------------------------------------------------------------------
//
//  Intan Technologies RHX Data Acquisition Software
//  Version 3.0.3
//
//  Copyright (c) 2020-2021 Intan Technologies
//
//  This file is part of the Intan Technologies RHX Data Acquisition Software.
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//  This software is provided 'as-is', without any express or implied warranty.
//  In no event will the authors be held liable for any damages arising from
//  the use of this software.
//
//  See <http://www.intantech.com> for documentation and product information.
//
//------------------------------------------------------------------------------

#include <QtWidgets>
#include <QStringList>
#include "abstractrhxcontroller.h"
#include "referenceselectdialog.h"

using namespace std;

ReferenceSelectDialog::ReferenceSelectDialog(QString refString, SignalSources* signalSources_, QWidget* parent) :
    QDialog(parent),
    signalSources(signalSources_)
{
    hardwareReferenceRadioButton = new QRadioButton(tr("Hardware Reference"), this);
    portReferenceRadioButton = new QRadioButton(tr("Software Reference: All Enabled Channels on Port"), this);
    customReferenceRadioButton = new QRadioButton(tr("Software Reference: Custom Channel List"), this);

    connect(hardwareReferenceRadioButton, SIGNAL(clicked()), this, SLOT(hardwareButtonSelected()));
    connect(portReferenceRadioButton, SIGNAL(clicked()), this, SLOT(portReferenceButtonSelected()));
    connect(customReferenceRadioButton, SIGNAL(clicked()), this, SLOT(customReferenceButtonSelected()));

    referenceButtonGroup = new QButtonGroup(this);
    referenceButtonGroup->addButton(hardwareReferenceRadioButton, 0);
    referenceButtonGroup->addButton(portReferenceRadioButton, 1);
    referenceButtonGroup->addButton(customReferenceRadioButton, 2);

    QStringList portNameList;
    QStringList channelList;
    for (int port = 0; port < AbstractRHXController::maxNumSPIPorts(signalSources->getControllerType()); ++port) {
        QString portName = "PORT " + QString(QChar('A' + port));
        QStringList nameList = signalSources->amplifierNameListUserOrder(portName);
        if (!nameList.isEmpty()) {
            portNameList.append("Average of All Enabled Channels on Port " + QString(QChar('A' + port)));
            channelList.append(nameList);
        }
    }

    portComboBox = new QComboBox(this);
    portComboBox->addItem("Average of All Enabled Channels on All Ports");
    portComboBox->addItems(portNameList);
    if (!portNameList.isEmpty()) portComboBox->setCurrentIndex(1);

    QHBoxLayout* portLayout = new QHBoxLayout;
    portLayout->addWidget(portComboBox);
    portLayout->addStretch();

    channelListWidget = new QListWidget(this);
    channelListWidget->setSortingEnabled(false);
    for (int i = 0; i < channelList.size(); ++i) {
        QString channelName = channelList[i];
        QListWidgetItem* item = new QListWidgetItem(channelName);
        if (channelName.contains('(')) {
            channelName = channelName.section('(', 1, 1);
            channelName.chop(1);
        }
        Channel* channel = signalSources->channelByName(channelName);
        if (!channel->isEnabled()) item->setForeground(QBrush(Qt::gray));
        channelListWidget->addItem(item);
    }
    channelListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(channelListWidget, SIGNAL(itemSelectionChanged()), this, SLOT(selectedChannelsChanged()));

    QHBoxLayout* channelListLayout = new QHBoxLayout;
    channelListLayout->addWidget(channelListWidget);
    channelListLayout->addStretch();

    setWindowTitle(tr("Select Reference"));

    QVBoxLayout *boxLayout1 = new QVBoxLayout;
    boxLayout1->addWidget(hardwareReferenceRadioButton);
    boxLayout1->addWidget(new QLabel(tr("Use REF input on headstage as reference."), this));

    QLabel* label1 = new QLabel(tr("Use average of all enabled channels on a headstage port as reference."), this);
    label1->setWordWrap(true);

    QVBoxLayout *boxLayout2 = new QVBoxLayout;
    boxLayout2->addWidget(portReferenceRadioButton);
    boxLayout2->addWidget(label1);
    boxLayout2->addLayout(portLayout);

    QLabel* label2 = new QLabel(tr("Use single channel or average of multiple channels as reference.  "
                                   "Use Shift and Ctrl to select multiple channels.  "
                                   "Designated channels will be used even if disabled."), this);
    label2->setWordWrap(true);

    QVBoxLayout *boxLayout3 = new QVBoxLayout;
    boxLayout3->addWidget(customReferenceRadioButton);
    boxLayout3->addWidget(label2);
    boxLayout3->addLayout(channelListLayout);

    QGroupBox *mainGroupBox1 = new QGroupBox();
    mainGroupBox1->setLayout(boxLayout1);
    QGroupBox *mainGroupBox2 = new QGroupBox();
    mainGroupBox2->setLayout(boxLayout2);
    QGroupBox *mainGroupBox3 = new QGroupBox();
    mainGroupBox3->setLayout(boxLayout3);

    okButton = new QPushButton(tr("OK"), this);
    cancelButton = new QPushButton(tr("Cancel"), this);

    connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(mainGroupBox1);
    mainLayout->addWidget(mainGroupBox2);
    mainLayout->addWidget(mainGroupBox3);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);

    int numRefs = refString.count(QChar(',')) + 1;
    if (refString.toLower() == "hardware") {
        hardwareReferenceRadioButton->setChecked(true);
        hardwareButtonSelected();
    } else if (refString == "ALL" || refString.left(4) == "PORT") {
        portReferenceRadioButton->setChecked(true);
        portReferenceButtonSelected();
        if (refString == "ALL") {
            portComboBox->setCurrentIndex(0);
        } else {
            char portLetter = refString.back().toLatin1();
            for (int index = 1; index < portComboBox->count(); ++index) {
                if (portLetter == portComboBox->itemText(index).back().toLatin1()) {
                    portComboBox->setCurrentIndex(index);
                    break;
                }
            }
        }
    } else {
        customReferenceRadioButton->setChecked(true);
        customReferenceButtonSelected();
        for (int i = 0; i < numRefs; ++i) {
            QString ref = refString.section(',', i, i);
            QList<QListWidgetItem*> items = channelListWidget->findItems(ref, Qt::MatchContains);
            if (!items.isEmpty()) items[0]->setSelected(true);
        }
    }
}

QString ReferenceSelectDialog::referenceString() const
{
    QString refString;
    if (referenceButtonGroup->checkedId() == 0) {
        return QString("Hardware");
    } else if (referenceButtonGroup->checkedId() == 1) {
        int index = 0;
        if (portComboBox->currentIndex() > 0) {
            index = (int) (portComboBox->currentText().back().toLatin1() - 'A' + 1);
        }
        if (index == 0) {
            refString = "ALL";
        } else {
            refString = "PORT " + QString(QChar('A' + index - 1));
        }
    } else {
        QList<QListWidgetItem*> selected = channelListWidget->selectedItems();
        for (int i = 0; i < selected.size(); ++i) {
            QString channelName = selected[i]->text();
            if (channelName.contains('(')) {
                channelName = channelName.section('(', 1, 1);
                channelName.chop(1);
            }
            refString += channelName + ",";
        }
        refString.chop(1);  // Trim off last comma.
    }
    return refString;
}

int ReferenceSelectDialog::numSelectedChannels() const
{
    QList<QListWidgetItem*> selected = channelListWidget->selectedItems();
    return selected.size();
}

void ReferenceSelectDialog::hardwareButtonSelected()
{
    okButton->setEnabled(true);
    portComboBox->setEnabled(false);
    channelListWidget->setEnabled(false);
}

void ReferenceSelectDialog::portReferenceButtonSelected()
{
    okButton->setEnabled(true);
    portComboBox->setEnabled(true);
    channelListWidget->setEnabled(false);
}

void ReferenceSelectDialog::customReferenceButtonSelected()
{
    okButton->setEnabled(numSelectedChannels() > 0);
    portComboBox->setEnabled(false);
    channelListWidget->setEnabled(true);
}

void ReferenceSelectDialog::selectedChannelsChanged()
{
    okButton->setEnabled(numSelectedChannels() > 0);
}
