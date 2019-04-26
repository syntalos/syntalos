//  ------------------------------------------------------------------------
//
//  This file is part of the Intan Technologies RHD2000 Interface
//  Version 1.3
//  Copyright (C) 2013 Intan Technologies
//
//  ------------------------------------------------------------------------
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Lesser General Public License as published
//  by the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <QtGui>

#include "qtincludes.h"

#include "renamechanneldialog.h"

// Rename Channel dialog.
// This dialog allows users to enter a new name for the selected channel.
// A regular expression validator is used to enforce a 16-character limit
// so that the channel name appears correctly with limited screen space.

RenameChannelDialog::RenameChannelDialog(QString channel, QString oldName, QWidget *parent) :
    QDialog(parent)
{
    QVBoxLayout *mainLayout = new QVBoxLayout;
    QHBoxLayout *oldNameLayout = new QHBoxLayout;
    QHBoxLayout *newNameLayout = new QHBoxLayout;

    oldNameLayout->addWidget(new QLabel(tr("Old channel name: ").append(oldName)));

    nameLineEdit = new QLineEdit;

    QRegExp regExp("[\\S]{1,16}");  // name must be 1-16 non-whitespace characters
    nameLineEdit->setValidator(new QRegExpValidator(regExp, this));

    connect(nameLineEdit, SIGNAL(textChanged(const QString &)),
            this, SLOT(onLineEditTextChanged()));

    newNameLayout->addWidget(new QLabel(tr("New channel name:")));
    newNameLayout->addWidget(nameLineEdit);
    newNameLayout->addWidget(new QLabel(tr("(16 characters max)")));
    newNameLayout->addStretch();

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    mainLayout->addLayout(oldNameLayout);
    mainLayout->addLayout(newNameLayout);
    mainLayout->addWidget(buttonBox);
    //mainLayout->addStretch();

    setLayout(mainLayout);

    setWindowTitle(tr("Rename Channel ").append(channel));
}

// Enable OK button on valid name.
void RenameChannelDialog::onLineEditTextChanged()
{
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(
                nameLineEdit->hasAcceptableInput());
}
