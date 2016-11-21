/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MAZESCRIPT_H
#define MAZESCRIPT_H

#include <QObject>
#include <QTextStream>
#include <QElapsedTimer>

class QScriptEngine;
class Firmata;
class MazeIO;
class QFile;

class MazeScript : public QObject
{
    Q_OBJECT
public:
    explicit MazeScript(QObject *parent = 0);
    ~MazeScript();

    void initFirmata(const QString& serialDevice);

    void setScript(const QString& script);
    QString script() const;

    void setEventFile(const QString& fname);

public slots:
    void run();
    void stop();

signals:
    void evalError(int line, const QString& message);
    void firmataError(const QString& message);
    void mazeEvent(const QStringList& data);
    void finished();

private slots:
    void eventReceived(const QStringList& messages);

private:
    void resetEngine();
    
    Firmata *m_firmata;
    MazeIO *m_mazeio;
    QScriptEngine *m_jseng;

    QString m_script;

    QElapsedTimer m_timer;
    QString m_eventFileName;
    QFile *m_eventFile;
    bool m_running;
};

#endif // MAZESCRIPT_H
