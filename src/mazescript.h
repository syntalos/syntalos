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
class SerialFirmata;
class MazeIO;
class QFile;
class QProcess;

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

    void setExternalScript(QString path);
    QString externalScript() const;

    void setUseExternalScript(bool value);
    bool useExternalScript() const;

public slots:
    void run();
    void stop();

signals:
    void evalError(int line, const QString& message);
    void firmataError(const QString& message);
    void mazeEvent(const QStringList& data);
    void headersSet(const QStringList& headers);
    void finished();

private slots:
    void headersReceived(const QStringList& headers);
    void eventReceived(const QStringList& messages);

private:
    void resetEngine();
    
    SerialFirmata *m_firmata;
    MazeIO *m_mazeio;
    QScriptEngine *m_jseng;

    QString m_script;

    QElapsedTimer m_timer;
    QString m_eventFileName;
    QFile *m_eventFile;
    bool m_running;
    bool m_haveEvents;

    bool m_useExternalScript;
    QString m_externalScript;

    QProcess *m_externalProcess;
};

#endif // MAZESCRIPT_H
