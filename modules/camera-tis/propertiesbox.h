/*
 * Copyright 2021 The Imaging Source Europe GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "propertyworker.h"
#include "tcamcollection.h"

#include <QDialog>
#include <QLayout>
#include <QThread>
#include <string>

class Property;
namespace Ui
{
class PropertiesBox;
}

class PropertyTree : public QWidget
{
    Q_OBJECT

public:
    PropertyTree(const std::vector<Property*>& properties, QWidget* parent = nullptr);

private:
    void setup_ui();

    std::vector<Property*> m_properties;

    QVBoxLayout* p_layout = nullptr;
};

class PropertiesBox : public QWidget
{
    Q_OBJECT

public:
    explicit PropertiesBox(TcamCollection& collection, QWidget* parent = nullptr);
    ~PropertiesBox();

public slots:

    void notify_device_lost(const QString& info);
    void refresh();
    void update_tab(int);

    void keyPressEvent(QKeyEvent* event) override;

signals:

    void device_lost(const QString& info);
    void update_category(QString name);

private:
    void initialize_dialog(TcamCollection& collection);

    Ui::PropertiesBox* ui = nullptr;

    QThread* p_work_thread = nullptr;
    PropertyWorker* p_worker = nullptr;

    std::vector<Property*> m_properties;
};
