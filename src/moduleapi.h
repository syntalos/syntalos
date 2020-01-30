/*
 * Copyright (C) 2016-2020 Matthias Klumpp <matthias@tenstral.net>
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

#ifndef ABSTRACTMODULE_H
#define ABSTRACTMODULE_H

#include <QObject>
#include <QList>
#include <QByteArray>
#include <QAction>
#include <QPixmap>
#include <QJsonObject>
#include <QDebug>

#include "hrclock.h"
#include "streams/datatypes.h"
#include "optionalwaitcondition.h"

class AbstractModule;

/**
 * @brief The ModuleFeature flags
 * List of basic features this module may or may not support.
 */
enum class ModuleFeature {
    NONE = 0,
    RUN_EVENTS    = 1 << 0,
    RUN_THREADED  = 1 << 1,
    SHOW_SETTINGS = 1 << 2,
    SHOW_DISPLAY  = 1 << 3,
    SHOW_ACTIONS  = 1 << 4
};
Q_DECLARE_FLAGS(ModuleFeatures, ModuleFeature)
Q_DECLARE_OPERATORS_FOR_FLAGS(ModuleFeatures)

class ModuleInfo : public QObject
{
    Q_OBJECT
    friend class Engine;
public:
    explicit ModuleInfo(QObject *parent = nullptr);
    ~ModuleInfo();

    /**
     * @brief Name of this module used internally as unique identifier
     */
    virtual QString id() const;

    /**
     * @brief Name of this module displayed to the user
     */
    virtual QString name() const;

    /**
     * @brief Description of this module
     */
    virtual QString description() const;

    /**
     * @brief Additional licensing conditions that apply to this module.
     */
    virtual QString license() const;

    /**
     * @brief Icon of this module
     */
    virtual QPixmap pixmap() const;

    /**
     * @brief Returns true if only one instance of this module can exist.
     * @return True if singleton.
     */
    virtual bool singleton() const;

    /**
     * @brief Instantiate the actual module.
     * @return A new module of this type.
     */
    virtual AbstractModule *createModule(QObject *parent = nullptr) = 0;

    int count() const;

private:
    Q_DISABLE_COPY(ModuleInfo)
    class Private;
    QScopedPointer<Private> d;

    void setCount(int count);
};

#define ModuleInfoInterface_iid "com.draguhnlab.MazeAmaze.ModuleInfoInterface"
Q_DECLARE_INTERFACE(ModuleInfo, ModuleInfoInterface_iid)

/**
 * @brief The TestSubject struct
 * Data about a test subject.
 */
class TestSubject
{
public:
    QString id;
    QString group;
    bool active;
    QString comment;
    QVariant data;
};

/**
 * @brief Enum specifying directionality of a port (in or out)
 */
enum class PortDirection {
    NONE,
    INPUT,
    OUTPUT
};

class AbstractStreamPort
{
public:
    virtual QString id() const = 0;
    virtual QString title() const = 0;

    virtual PortDirection direction() const { return PortDirection::NONE; };

    virtual int dataTypeId() const = 0;
    virtual QString dataTypeName() const = 0;
};

class StreamInputPort : public AbstractStreamPort
{
public:
    explicit StreamInputPort(const QString &id, const QString &title);
    ~StreamInputPort();

    template<typename T>
    void setAcceptedType()
    {
        m_acceptedTypeId = qMetaTypeId<T>();
        m_acceptedTypeName = QMetaType::typeName(m_acceptedTypeId);
    }

    int dataTypeId() const override;
    QString dataTypeName() const override;

    bool acceptsSubscription(const QString &typeName);
    bool hasSubscription() const;
    void setSubscription(std::shared_ptr<VariantStreamSubscription> sub);
    void resetSubscription();

    template<typename T>
    std::shared_ptr<StreamSubscription<T>> subscription()
    {
        auto sub = std::dynamic_pointer_cast<StreamSubscription<T>>(m_sub.value());
        if (sub == nullptr) {
            if (hasSubscription()) {
                qCritical().noquote() << "Conversion of variant subscription to dedicated type" << typeid(T).name() << "failed."
                                      << "Modules are connected in a way they shouldn't be, will probably crash now.";
                assert(0);
            } else {
                qWarning().noquote() << "Tried to obtain" << typeid(T).name() << "subscription from a port that was not subscribed to anything.";
            }
        }
        return sub;
    }

    std::shared_ptr<VariantStreamSubscription> subscriptionVar();

    QString id() const override;
    QString title() const override;
    PortDirection direction() const override;

private:
    Q_DISABLE_COPY(StreamInputPort)
    class Private;
    QScopedPointer<Private> d;
    int m_acceptedTypeId;
    QString m_acceptedTypeName;
    std::optional<std::shared_ptr<VariantStreamSubscription>> m_sub;
};

class StreamOutputPort : public AbstractStreamPort
{
public:
    explicit StreamOutputPort(const QString &id, const QString &title, std::shared_ptr<VariantDataStream> stream);
    ~StreamOutputPort();

    bool canSubscribe(const QString &typeName);
    int dataTypeId() const override;
    QString dataTypeName() const override;

    template<typename T>
    std::shared_ptr<DataStream<T>> stream() { return std::dynamic_pointer_cast<DataStream<T>>(streamVar()); }
    std::shared_ptr<VariantDataStream> streamVar();

    std::shared_ptr<VariantStreamSubscription> subscribe();

    void startStream();
    void stopStream();

    QString id() const override;
    QString title() const override;
    PortDirection direction() const override;

private:
    Q_DISABLE_COPY(StreamOutputPort)
    class Private;
    QScopedPointer<Private> d;
};

class AbstractModule : public QObject
{
    Q_OBJECT
    friend class Engine;
public:
    explicit AbstractModule(QObject *parent = nullptr);
    ~AbstractModule();

    ModuleState state() const;

    /**
     * @brief Allow a running module to mark itself as idle.
     *
     * This may be useful in case the module us missing an essential stream
     * connection and essentially does nothing in a run.
     */
    void setStateIdle();

    /**
     * @brief Allow a preparing module to mark itself as ready
     *
     * When a module is done preparing itself, e.g. by setting up its thread,
     * it can switch to its ready state on its own and thereby allow the engine
     * to start it. Usually doing this explicitly is not necessary.
     */
    void setStateReady();

    /**
     * @brief Name of this module used internally as unique identifier
     */
    QString id() const;

    /**
     * @brief Name of this module displayed to the user
     */
    virtual QString name() const;
    virtual void setName(const QString& name);

    /**
     * @brief Return a bitfield of features this module supports.
     */
    virtual ModuleFeatures features() const;

    /**
     * @brief Register an output port for this module
     *
     * This function should be called in the module's constructor to publish the intent
     * to produce an output stream of type T. Other modules may subscribe to this stream.
     *
     * @returns The data stream instance for this module to write to.
     */
    template<typename T>
    std::shared_ptr<DataStream<T>> registerOutputPort(const QString &id, const QString &title = QString())
    {
        if (m_outPorts.contains(id))
            qWarning() << "Module" << name() << "already registered an output port with ID:" << id;

        std::shared_ptr<DataStream<T>> stream(new DataStream<T>());
        std::shared_ptr<StreamOutputPort> outPort(new StreamOutputPort(id, title, stream));
        m_outPorts.insert(id, outPort);
        return stream;
    }

    /**
     * @brief Register an input port for this module
     *
     * This function should be called in the module's constructor to publish the intent
     * to accept input stream subscriptions of type T. The user may subscribe this module
     * to other modules which produce the data it accepts.
     *
     * @returns A StreamInputPort instance, which can be checked for subscriptions
     */
    template<typename T>
    std::shared_ptr<StreamInputPort> registerInputPort(const QString &id, const QString &title = QString())
    {
        if (m_inPorts.contains(id))
            qWarning() << "Module" << name() << "already registered an output port with ID:" << id;

        std::shared_ptr<StreamInputPort> inPort(new StreamInputPort(id, title));
        inPort->setAcceptedType<T>();
        m_inPorts.insert(id, inPort);
        return inPort;
    }

    /**
     * @brief Initialize the module
     *
     * Initialize this module. This method is called once after construction,
     * and can be used to initialize parts of the module that may fail and may need
     * to emit an error message (unlike in the constructor, which always succeeds)
     * @return true if success
     */
    virtual bool initialize();

    /**
     * @brief Prepare for an experiment run
     *
     * Prepare this module to run. This method is called once
     * prior to every experiment run.
     * @return true if success
     */
    virtual bool prepare(const QString& storageRootDir, const TestSubject& testSubject) = 0;

    /**
     * @brief Run when the experiment is started and the HRTimer has an initial time set.
     * Switches the module into "Started" mode.
     */
    virtual void start();

    /**
     * @brief Run task in a thread
     *
     * If the module advertises itself has being threaded, this function is executed
     * in a new thread after the module has left its PREPARE stage.
     * The module should only start to handle input when all modules are ready. This is the case
     * when either the start() method was called, a start event was sent via the
     * system status event stream or the wait condition unblocks (call waitCondition->wait(this)
     * to wait for the start signal).
     * @return true if no error
     */
    virtual void runThread(OptionalWaitCondition *startWaitCondition);

    /**
     * @brief Execute actions in main thread event loop when idle
     *
     * This function is run in the main event loop of the application, once
     * per iteration (when idle). This function must never block for a long time,
     * to allow other modules to be executed as well.
     * It can access UI elements of the application though, and gets all other
     * benefits from running in the same thread as the UI.
     * @return true if no error
     */
    virtual bool runEvent();

    /**
     * @brief Stop running an experiment.
     * Stop execution of an experiment. This method is called after
     * prepare() was run.
     */
    virtual void stop();

    /**
     * @brief Finalize this module.
     * This method is called before the module itself is destroyed.
     */
    virtual void finalize();

    /**
     * @brief Show the display widgets of this module
     */
    virtual void showDisplayUi();
    virtual bool isDisplayUiVisible();

    /**
     * @brief Show the configuration UI of this module
     */
    virtual void showSettingsUi();
    virtual bool isSettingsUiVisible();

    /**
     * @brief Hide all display widgets of this module
     */
    virtual void hideDisplayUi();

    /**
     * @brief Hide the configuration UI of this module
     */
    virtual void hideSettingsUi();

    /**
     * @brief Get actions to add to the module's submenu
     * @return
     */
    virtual QList<QAction*> actions();

    /**
     * @brief Serialize the settings of this module to a byte array.
     */
    virtual QByteArray serializeSettings(const QString& confBaseDir);

    /**
     * @brief Load settings from previously stored data.
     * @return true if successful.
     */
    virtual bool loadSettings(const QString& confBaseDir, const QByteArray& data);

    /**
     * @brief Return last error
     * @return The last error message generated by this module
     */
    QString lastError() const;

    QList<std::shared_ptr<StreamInputPort>> inPorts() const;
    QList<std::shared_ptr<StreamOutputPort>> outPorts() const;

    QJsonValue serializeDisplayUiGeometry();
    void restoreDisplayUiGeometry(QJsonObject info);

    void setTimer(std::shared_ptr<HRTimer> timer);

signals:
    void actionsUpdated();
    void stateChanged(ModuleState state);
    void error(const QString& message);
    void statusMessage(const QString& message);
    void nameChanged(const QString& name);

protected:
    void raiseError(const QString& message);
    QByteArray jsonObjectToBytes(const QJsonObject& object);
    QJsonObject jsonObjectFromBytes(const QByteArray& data);

    void setStatusMessage(const QString& message);
    bool makeDirectory(const QString &dir);

    /**
     * @brief Register a display window for this module.
     * @param window
     * @param owned
     *
     * Add a new display window to this module.
     * The window instance will be deleted on destruction, if
     * #own was set to true.
     */
    void addDisplayWindow(QWidget *window, bool owned = true);

    /**
     * @brief Register a settings window for this module.
     * @param window
     * @param owned
     *
     * Add a new settings window to this module.
     * The window instance will be deleted on destruction, if
     * #own was set to true.
     */
    void addSettingsWindow(QWidget *window, bool owned = true);

    void setInitialized();
    bool initialized() const;

    QString m_storageDir;

    std::atomic_bool m_running;

    std::shared_ptr<HRTimer> m_timer;

private:
    Q_DISABLE_COPY(AbstractModule)
    class Private;
    QScopedPointer<Private> d;

    QMap<QString, std::shared_ptr<StreamOutputPort>> m_outPorts;
    QMap<QString, std::shared_ptr<StreamInputPort>> m_inPorts;

    void setState(ModuleState state);
    void setId(const QString &id);
};

#endif // ABSTRACTMODULE_H
