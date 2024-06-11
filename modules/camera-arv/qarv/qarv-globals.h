#ifndef GLOBALS_H
#define GLOBALS_H

#include <gio/gio.h>  // Workaround for gdbusintrospection's use of "signal".
#include <QDebug>
#include <QStandardItemModel>
#include <cmath>

namespace QArv
{

class MessageSender : public QObject {
public:
    Q_OBJECT

signals:
    void newDebugMessage(const QString &scope, const QString &message);

private:
    bool connected;
    QList<QPair<QString, QString>> preconnectMessages;

    MessageSender();
    void connectNotify(const QMetaMethod& signal) override;
    void disconnectNotify(const QMetaMethod& signal) override;

    void sendMessage(const QString& scope, const QString& message);

    friend class QArvDebug;
};

class QArvDebug : public QDebug {
public:

    ~QArvDebug();
    static MessageSender messageSender;

    QArvDebug(const QString &modId) :
          QDebug(&m_message), m_modId(modId) {}

private:
    QString m_modId;
    QString m_message;
};

const int slidersteps = 1000;

static inline double slider2value_log(int slidervalue,
                                      QPair<double, double>& range) {
    double value = log2(range.second) - log2(range.first);
    return exp2(value * slidervalue / slidersteps + log2(range.first));
}

static inline int value2slider_log(double value,
                                   QPair<double, double>& range) {
    return slidersteps
           * (log2(value) - log2(range.first))
           / (log2(range.second) - log2(range.first));
}

template <typename T>
static inline QVariant ptr2var(const T* ptr) {
    return QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(ptr));
}

template <typename T>
static inline T* var2ptr(const QVariant& val) {
    return reinterpret_cast<T*>(val.value<quintptr>());
}

}

#endif
