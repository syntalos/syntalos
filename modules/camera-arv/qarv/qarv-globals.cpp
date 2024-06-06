#include "qarv-globals.h"

#include <QTime>

using namespace QArv;

MessageSender QArvDebug::messageSender __attribute__((init_priority(1000)));

MessageSender::MessageSender() : QObject(), connected(false) {}

void MessageSender::connectNotify(const QMetaMethod& signal) {
    QObject::connectNotify(signal);
    connected = true;
    foreach (const QString& msg, preconnectMessages)
    emit newDebugMessage(msg);
    preconnectMessages.clear();
}

void MessageSender::disconnectNotify(const QMetaMethod& signal) {
    QObject::disconnectNotify(signal);
    if (receivers(SIGNAL(newDebugMessage(QString))) < 1)
        connected = false;
}

void MessageSender::sendMessage(const QString& message) {
    if (connected)
        emit newDebugMessage(message);
    else
        preconnectMessages << message;
}

QArvDebug::~QArvDebug() {
    auto now = QTime::currentTime().toString("[hh:mm:ss] ");
    foreach (auto line, message.split('\n')) {
        if (line.startsWith('"')) {
            auto lineref = line.midRef(1, line.length() - 3);
            qDebug(prepend ? "QArv %s%s" : "%s%s",
                   now.toLocal8Bit().constData(),
                   lineref.toLocal8Bit().constData());
            messageSender.sendMessage(now + lineref.toString());
        } else {
            qDebug(prepend ? "QArv %s%s" : "%s%s",
                   now.toLocal8Bit().constData(),
                   line.toLocal8Bit().constData());
            messageSender.sendMessage(now + line);
        }
    }
}
