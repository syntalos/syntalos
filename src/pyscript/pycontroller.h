#ifndef PYCONTROLLER_H
#define PYCONTROLLER_H

#include <QObject>

class SerialFirmata;
class MaIO;
class QThread;
class PyWorker;

class PyController : public QObject
{
    Q_OBJECT
public:
    explicit PyController(QObject *parent = nullptr);
    ~PyController();

    MaIO *maio() const;

    void initFirmata(const QString& serialDevice);
    void setScriptContent(const QString& script);

    void startScript();
    void terminateScript();

    bool isRunning() const;

signals:
    void scriptError(const QString& message);
    void firmataError(const QString& message);

private slots:
    void pyThreadFinished();

private:
    QThread *m_pyThread;
    PyWorker *m_worker;
    bool m_running;

    SerialFirmata *m_firmata;
    QString m_script;
};

#endif // PYCONTROLLER_H
