#ifndef PYTHREAD_H
#define PYTHREAD_H

#include <QObject>
#include <QThread>

class SerialFirmata;
class MaIO;

class PyThread : public QThread
{
    Q_OBJECT
public:
    explicit PyThread(QObject *parent = nullptr);

    MaIO *maio() const;

    void initFirmata(const QString& serialDevice);
    void setScriptContent(const QString& script);

public slots:
    void start(QThread::Priority priority = InheritPriority);
    void quit();

signals:
    void scriptError(const QString& message);
    void firmataError(const QString& message);

private:
    void run() override;

    SerialFirmata *m_firmata;
    QString m_script;

    bool m_terminating;
    bool m_initializing;
};

#endif // PYTHREAD_H
