/*
 * Copyright (C) 2016-2019 Matthias Klumpp <matthias@tenstral.net>
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
#include "modulemanager.h"

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
    friend class ModuleManager;
public:
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
    int m_count;
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

class StreamInputPort
{
public:
    explicit StreamInputPort(const QString &id, const QString &title);

    template<typename T>
    void setAcceptedType()
    {
        m_acceptedTypeName = QMetaType::typeName(qMetaTypeId<T>());
    }

    QString acceptedTypeName() const;

    bool acceptsSubscription(const QString &typeName);
    bool hasSubscription() const;
    void setSubscription(std::shared_ptr<VariantStreamSubscription> sub);
    void resetSubscription();

    template<typename T>
    std::shared_ptr<StreamSubscription<T>> subscription()
    {
        return m_sub;
    }

    QString id() const;
    QString title() const;

private:
    QString m_id;
    QString m_title;
    QString m_acceptedTypeName;
    std::optional<std::shared_ptr<VariantStreamSubscription>> m_sub;
};

class StreamOutputPort
{
public:
    explicit StreamOutputPort(const QString &id, const QString &title, std::shared_ptr<VariantDataStream> stream);

    bool canSubscribe(const QString &typeName);
    QString dataTypeName() const;

    template<typename T>
    std::shared_ptr<DataStream<T>> stream()
    {
        return m_stream;
    }

    QString id() const;
    QString title() const;

private:
    QString m_id;
    QString m_title;
    std::shared_ptr<VariantDataStream> m_stream;
};

class AbstractModule : public QObject
{
    Q_OBJECT
    friend class ModuleManager;
public:
    explicit AbstractModule(QObject *parent = nullptr);

    ModuleState state() const;
    void setState(ModuleState state);

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
     * Initialize this module. This method is called once after construction.
     * @return true if success
     */
    virtual bool initialize(ModuleManager *manager) = 0;

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
     * @brief Run task in a thread
     *
     * If the module advertises itself has being threaded, this function is executed
     * in a new thread after the module has left its PREPARE stage.
     * The module should only start to handle input when an actual START command was issued,
     * either via the start() method being called or via a start event being sent via the
     * system status event stream.
     * @return true if no error
     */
    virtual void runThread();

    /**
     * @brief Stop running an experiment.
     * Stop execution of an experiment. This method is called after
     * prepare() was run.
     */
    virtual void stop() = 0;

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

    /**
     * @brief Check if the selected module can be removed.
     * This function is called by the module manager prior to removal of a module on
     * each active module. If False is returned, the module is prevented from being removed.
     * @return True if module can be removed, fals if removal should be prevented.
     */
    virtual bool canRemove(AbstractModule *mod);

    /**
     * @brief Subscribe to this modules' status message stream
     *
     * This method is used by the engine to read any messages emitted by this module quickly
     * and in a threadsafe way.
     */
    std::shared_ptr<StreamSubscription<ModuleMessage> > getMessageSubscription();

    /**
     * @brief Subscribe this module to receive system events
     *
     * This method is used by the engine to allow this module to read events that are broadcasted
     * system-wide to all modules.
     */
    void subscribeToSysEvents(std::shared_ptr<StreamSubscription<SystemStatusEvent> > subscription);

    QList<std::shared_ptr<StreamInputPort>> inPorts() const;
    QList<std::shared_ptr<StreamOutputPort>> outPorts() const;

    bool makeDirectory(const QString &dir);

    void setInitialized();
    bool initialized() const;

    QJsonValue serializeDisplayUiGeometry();
    void restoreDisplayUiGeomatry(QJsonObject info);

    void setStatusMessage(const QString& message);

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

    QString m_name;
    QString m_storageDir;

    std::shared_ptr<HRTimer> m_timer;

    QList<QWidget*> m_displayWindows;
    QList<QWidget*> m_settingsWindows;

    std::optional<std::shared_ptr<StreamSubscription<SystemStatusEvent> > > m_sysEventsSub;

private:
    ModuleState m_state;
    QString m_lastError;
    QString m_id;
    std::unique_ptr<DataStream<ModuleMessage>> m_msgStream;
    QMap<QString, std::shared_ptr<StreamOutputPort>> m_outPorts;
    QMap<QString, std::shared_ptr<StreamInputPort>> m_inPorts;
    bool m_initialized;

    void setId(const QString &id);
};

#endif // ABSTRACTMODULE_H
