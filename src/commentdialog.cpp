/*
 * Copyright (C) 2022-2023 Matthias Klumpp <matthias@tenstral.net>
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

#include "commentdialog.h"
#include "ui_commentdialog.h"

#include <QMessageBox>

CommentDialog::CommentDialog(Engine *engine, QWidget *parent)
    : QDialog(parent),
      ui(new Ui::CommentDialog)
{
    ui->setupUi(this);
    m_engine = engine;
    setWindowTitle("Add User Comment to Experiment Run");

    ui->editInfoLabel->setElideMode(Qt::ElideLeft);
    ui->radioBtnCommentNext->setChecked(true);
    on_radioBtnCommentNext_toggled(true);
    if (m_engine->lastRunExportDir().isEmpty())
        ui->radioBtnCommentLast->setEnabled(false);

    ui->commentTextEdit->setFocus();
}

CommentDialog::~CommentDialog()
{
    delete ui;
}

void CommentDialog::closeEvent(QCloseEvent *event)
{
    if (ui->commentTextEdit->toPlainText().isEmpty()) {
        event->accept();
        return;
    }

    auto reply = QMessageBox::question(
        this,
        "Discard comment?",
        QStringLiteral("Do you want to discard the entered text and not save it for the selected run?"),
        QMessageBox::Yes | QMessageBox::No
    );
    if (reply == QMessageBox::No) {
        event->ignore();
        return;
    }

    event->accept();
}

void CommentDialog::on_radioBtnCommentNext_toggled(bool checked)
{
    if (!checked)
        return;

    ui->editInfoLabel->setText("next experiment run");
    ui->commentTextEdit->setPlainText(m_engine->readRunComment());
}

void CommentDialog::on_radioBtnCommentLast_toggled(bool checked)
{
    if (!checked)
        return;

    const auto lastRunDir = m_engine->lastRunExportDir();
    ui->editInfoLabel->setText(lastRunDir);
    ui->commentTextEdit->setPlainText(m_engine->readRunComment(lastRunDir));
}

void CommentDialog::on_buttonBox_accepted()
{
    auto text = ui->commentTextEdit->toPlainText();
    if (text.isEmpty())
        return;

    if (ui->radioBtnCommentNext->isChecked()) {
        m_engine->setRunComment(text);
    } else {
        m_engine->setRunComment(text, m_engine->lastRunExportDir());
    }
}

void CommentDialog::on_buttonBox_clicked(QAbstractButton *button)
{
    const auto role = ui->buttonBox->buttonRole(button);
    if (role == QDialogButtonBox::ApplyRole)
        ui->buttonBox->accepted();
    else if (role == QDialogButtonBox::DestructiveRole)
        ui->buttonBox->rejected();
}
