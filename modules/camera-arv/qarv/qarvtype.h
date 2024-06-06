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

#ifndef QARVTYPE_H
#define QARVTYPE_H

#include <gio/gio.h>  // Workaround for gdbusintrospection's use of "signal".

#pragma GCC visibility push(default)

#include <QWidget>
#include <QMetaType>

//! QArvEditor is a QWidget that contains the actual editing widgets.
/*! It is used to translate whichever signal is emitted by the actual widgets
 * when editig is finished into the editingFinished() signal which can be
 * used by QArvCameraDelegate.
 */
struct QArvEditor : QWidget {
    Q_OBJECT

public:
    QArvEditor(QWidget* parent = 0);

signals:
    void editingFinished();

private slots:
    void editingComplete() { emit editingFinished(); }

    friend struct QArvEnumeration;
    friend struct QArvString;
    friend struct QArvFloat;
    friend struct QArvInteger;
    friend struct QArvBoolean;
    friend struct QArvCommand;
    friend struct QArvRegister;
};

//! These types are used by the QArvCamera model and delegate to edit feature node values.
/*! Sometimes, a feature has several possible types (e.g. an
 * enumeration can be either an enumeration, a string or an integer; an integer
 * can be cast to a float etc.), but the delegate needs to be able to identify
 * the type exactly. Therefore, each type is given a distinct class.
 * When deciding what type to return, the model tries to match the
 * highest-level type. Each type also provides its own editing widget.
 */
struct QArvType {
    virtual ~QArvType() {}
    virtual operator QString() const = 0;
    virtual QArvEditor* createEditor(QWidget* parent = NULL) const = 0;
    virtual void populateEditor(QWidget* editor) const = 0;
    virtual void readFromEditor(QWidget* editor) = 0;
};

//! \name Types that correspond to types of feature nodes
/**@{*/

struct QArvEnumeration : QArvType {
    QList<QString> names;
    QList<QString> values;
    QList<bool> isAvailable;
    int currentValue;
    QArvEnumeration();
    operator QString()  const override;
    QArvEditor* createEditor(QWidget* parent) const override;
    void populateEditor(QWidget* editor) const override;
    void readFromEditor(QWidget* editor) override;
};

struct QArvString : QArvType {
    QString value;
    qint64 maxlength;
    QArvString();
    operator QString() const override;
    QArvEditor* createEditor(QWidget* parent) const override;
    void populateEditor(QWidget* editor) const override;
    void readFromEditor(QWidget* editor) override;
};

struct QArvFloat : QArvType {
    double value, min, max;
    QString unit;
    QArvFloat();
    operator QString() const override;
    QArvEditor* createEditor(QWidget* parent) const override;
    void populateEditor(QWidget* editor) const override;
    void readFromEditor(QWidget* editor) override;
};

struct QArvInteger : QArvType {
    qint64 value, min, max, inc;
    operator QString() const override;
    QArvEditor* createEditor(QWidget* parent) const override;
    void populateEditor(QWidget* editor) const override;
    void readFromEditor(QWidget* editor) override;
};

struct QArvBoolean : QArvType {
    bool value;
    operator QString() const override;
    QArvEditor* createEditor(QWidget* parent) const override;
    void populateEditor(QWidget* editor) const override;
    void readFromEditor(QWidget* editor) override;
};

struct QArvCommand : QArvType {
    operator QString() const override;
    QArvEditor* createEditor(QWidget* parent) const override;
    void populateEditor(QWidget* editor) const override;
    void readFromEditor(QWidget* editor) override;
};

struct QArvRegister : QArvType {
    QByteArray value;
    qint64 length;
    QArvRegister();
    operator QString() const override;
    QArvEditor* createEditor(QWidget* parent) const override;
    void populateEditor(QWidget* editor) const override;
    void readFromEditor(QWidget* editor) override;
};

/**@}*/

Q_DECLARE_METATYPE(QArvType*)
Q_DECLARE_METATYPE(QArvEnumeration)
Q_DECLARE_METATYPE(QArvString)
Q_DECLARE_METATYPE(QArvFloat)
Q_DECLARE_METATYPE(QArvInteger)
Q_DECLARE_METATYPE(QArvBoolean)
Q_DECLARE_METATYPE(QArvCommand)
Q_DECLARE_METATYPE(QArvRegister)

#pragma GCC visibility pop

#endif
