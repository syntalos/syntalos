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

#include "renamechanneldialog.h"

RenameChannelDialog::RenameChannelDialog(const QString& nativeName, const QString& oldName, QWidget* parent) :
    QDialog(parent)
{
    QString addNativeName;
    if (nativeName != oldName) addNativeName = " (" + nativeName + ")";

    QHBoxLayout* oldNameLayout = new QHBoxLayout;
    oldNameLayout->addWidget(new QLabel(tr("Old channel name: ") + oldName + addNativeName, this));

    nameLineEdit = new QLineEdit;
    QRegExp regExp("[\\w-\\+\\./]{1,16}");  // Name must be 1-16 characters, alphanumeric or _-+./
    nameLineEdit->setValidator(new QRegExpValidator(regExp, this));

    connect(nameLineEdit, SIGNAL(textChanged(const QString &)),
            this, SLOT(onLineEditTextChanged()));

    QHBoxLayout* newNameLayout = new QHBoxLayout;
    newNameLayout->addWidget(new QLabel(tr("New channel name:"), this));
    newNameLayout->addWidget(nameLineEdit);
    newNameLayout->addWidget(new QLabel(tr("(16 characters max)"), this));
    newNameLayout->addStretch();

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->addLayout(oldNameLayout);
    mainLayout->addLayout(newNameLayout);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);

    setWindowTitle(tr("Rename Channel"));
}

// Enable OK button on valid name.
void RenameChannelDialog::onLineEditTextChanged()
{
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(nameLineEdit->hasAcceptableInput());
}

