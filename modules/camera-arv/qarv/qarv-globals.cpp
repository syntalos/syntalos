#include "qarv-globals.h"

#include <QTime>

using namespace QArv;

MessageSender QArvDebug::messageSender __attribute__((init_priority(1000)));

MessageSender::MessageSender() : QObject(), connected(false) {}

void MessageSender::connectNotify(const QMetaMethod& signal) {
    QObject::connectNotify(signal);
    connected = true;
    for (auto& pair : preconnectMessages)
        emit newDebugMessage(pair.first, pair.second);
    preconnectMessages.clear();
}

void MessageSender::disconnectNotify(const QMetaMethod& signal) {
    QObject::disconnectNotify(signal);
    if (receivers(SIGNAL(newDebugMessage(QString))) < 1)
        connected = false;
}

void MessageSender::sendMessage(const QString &scope, const QString& message) {
    if (connected)
        emit newDebugMessage(scope, message);
    else
        preconnectMessages << qMakePair(scope, message);
}

QArvDebug::~QArvDebug() {
    if (m_modLog == nullptr) {
        auto now = QTime::currentTime().toString("[hh:mm:ss] ");
        foreach (auto line, m_message.split('\n')) {
            if (line.startsWith('"')) {
                auto lineref = QStringView{line}.mid(1, line.length() - 3);
                qDebug("%s%s",
                       now.toLocal8Bit().constData(),
                       lineref.toLocal8Bit().constData());
                messageSender.sendMessage("main", now + lineref.toString());
            } else {
                qDebug("%s%s",
                       now.toLocal8Bit().constData(),
                       line.toLocal8Bit().constData());
                messageSender.sendMessage("main", now + line);
            }
        }
    } else {
        const auto scope = QString::fromStdString(m_modLog->get_logger_name());
        foreach (auto line, m_message.split('\n')) {
            if (line.startsWith('"')) {
                auto lineref = QStringView{line}.mid(1, line.length() - 3);
                LOG_INFO(m_modLog, "{}", lineref.toLocal8Bit().constData());
                messageSender.sendMessage(scope, lineref.toString());
            } else {
                LOG_INFO(m_modLog, "{}", line.toLocal8Bit().constData());
                messageSender.sendMessage(scope, line);
            }
        }
    }
}
