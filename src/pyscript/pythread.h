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

    void setFirmata(SerialFirmata *firmata);
    MaIO *maio();

    void runScript();
    void terminate();

    void setScriptContent(const QString& script);


signals:
    void errorReceived(const QString& message);

private:
    void run() override;

    QString m_script;
    bool m_terminating;
};

#endif // PYTHREAD_H
