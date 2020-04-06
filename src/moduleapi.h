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

#ifndef MODULEAPI_H
#define MODULEAPI_H

#include <QObject>
#include <QList>
#include <QByteArray>
#include <QAction>
#include <QPixmap>
#include <QJsonObject>
#include <QDebug>

#include "syclock.h"
#include "timesync.h"
#include "streams/datatypes.h"
#include "optionalwaitcondition.h"
#include "edlstorage.h"

namespace Syntalos {

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
     * @brief The dominant color for this module.
     */
    virtual QColor color() const;

    /**
     * @brief Name of a storage group for modules of this type
     *
     * If this is left empty, modules will write into the main data
     * storage group instead of a dedicated one.
     */
    virtual QString storageGroupName() const;

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

} // end Syntalos of namespace

#define ModuleInfoInterface_iid "com.draguhnlab.MazeAmaze.ModuleInfoInterface"
Q_DECLARE_INTERFACE(Syntalos::ModuleInfo, ModuleInfoInterface_iid)

namespace Syntalos {

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

    virtual AbstractModule *owner() const = 0;
};

class StreamOutputPort;
class VarStreamInputPort : public AbstractStreamPort
{
public:
    explicit VarStreamInputPort(AbstractModule *owner, const QString &id, const QString &title);
    ~VarStreamInputPort();

    virtual bool acceptsSubscription(const QString &typeName) = 0;
    bool hasSubscription() const;
    void setSubscription(StreamOutputPort *src, std::shared_ptr<VariantStreamSubscription> sub);
    void resetSubscription();
    StreamOutputPort *outPort() const;

    std::shared_ptr<VariantStreamSubscription> subscriptionVar();

    QString id() const override;
    QString title() const override;
    PortDirection direction() const override;
    AbstractModule *owner() const override;

protected:
    std::optional<std::shared_ptr<VariantStreamSubscription>> m_sub;

private:
    Q_DISABLE_COPY(VarStreamInputPort)
    class Private;
    QScopedPointer<Private> d;
};

template<typename T>
class StreamInputPort : public VarStreamInputPort
{
public:
    explicit StreamInputPort(AbstractModule *owner, const QString &id, const QString &title)
        : VarStreamInputPort(owner, id, title)
    {
        m_acceptedTypeId = qMetaTypeId<T>();
        m_acceptedTypeName = QMetaType::typeName(m_acceptedTypeId);
    }

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

    int dataTypeId() const override
    {
        return m_acceptedTypeId;
    }

    QString dataTypeName() const override
    {
        return m_acceptedTypeName;
    }

    bool acceptsSubscription(const QString &typeName) override
    {
        return m_acceptedTypeName == typeName;
    }

private:
    int m_acceptedTypeId;
    QString m_acceptedTypeName;
};

class StreamOutputPort : public AbstractStreamPort
{
public:
    explicit StreamOutputPort(AbstractModule *owner, const QString &id, const QString &title, std::shared_ptr<VariantDataStream> stream);
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
    AbstractModule *owner() const override;

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
     * @brief Index of this module.
     *
     * The index is usually the count of modules of the same kind.
     * E.g. the first "camera" module may have an index of 1, the second of 2, etc.
     */
    int index() const;

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
        std::shared_ptr<StreamOutputPort> outPort(new StreamOutputPort(this, id, title, stream));
        stream->setCommonMetadata(this->id(), name(), title);
        m_outPorts.insert(id, outPort);

        Q_EMIT portConfigurationUpdated();
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
    std::shared_ptr<StreamInputPort<T>> registerInputPort(const QString &id, const QString &title = QString())
    {
        if (m_inPorts.contains(id))
            qWarning() << "Module" << name() << "already registered an output port with ID:" << id;

        std::shared_ptr<StreamInputPort<T>> inPort(new StreamInputPort<T>(this, id, title));
        m_inPorts.insert(id, inPort);

        Q_EMIT portConfigurationUpdated();
        return inPort;
    }

    /**
     * @brief Register an output port for this module using a type ID
     *
     * This function permits registering an output port for a particular type when only knowing
     * its type ID. This is usefuly when dynamically allocating ports.
     *
     * @returns A variant data stream instance for this module to write to.
     */
    std::shared_ptr<VariantDataStream> registerOutputPortByTypeId(int typeId, const QString &id, const QString &title = QString())
    {
        if (m_outPorts.contains(id))
            qWarning() << "Module" << name() << "already registered an output port with ID:" << id;

        auto varStream = newStreamForType(typeId);
        if (varStream == nullptr)
            return nullptr;

        std::shared_ptr<VariantDataStream> stream(varStream);
        std::shared_ptr<StreamOutputPort> outPort(new StreamOutputPort(this, id, title, stream));
        stream->setCommonMetadata(this->id(), name(), title);
        m_outPorts.insert(id, outPort);
        Q_EMIT portConfigurationUpdated();
        return stream;
    }

    /**
     * @brief Register an input port for this module using a type ID
     *
     * @returns A VarStreamInputPort instance, which can be checked for subscriptions
     */
    std::shared_ptr<VarStreamInputPort> registerInputPortByTypeId(int typeId, const QString &id, const QString &title = QString())
    {
        if (m_inPorts.contains(id))
            qWarning() << "Module" << name() << "already registered an output port with ID:" << id;

        auto varInPort = newInputPortForType(typeId, this, id, title);
        if (varInPort == nullptr)
            return nullptr;

        std::shared_ptr<VarStreamInputPort> inPort(varInPort);
        m_inPorts.insert(id, inPort);
        Q_EMIT portConfigurationUpdated();
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
    virtual bool prepare(const TestSubject& testSubject) = 0;

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
     * @brief Execute actions in thread event loop when idle
     *
     * This function is run together with other modules in one thread, and is called upon periodically.
     * Therefore, function must never block for a long time, to allow other modules to be executed as well.
     * Keep in mind that this function may be called from any thread and may even be moved between threads
     * depending on system load, so do not assume you will always be in your own thread or never move between
     * threads during execution.
     *
     * This is ideal to perform more light-weight tasks that do not need to run in their own thread, to increase
     * utilization of system resources and reduce theading overhead caused by smaller modules.
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
     * @brief Retrieve module metadata specific for this experiment.
     *
     * The module may have generated some additional metadata that should be stored with the
     * other experiment data.
     * This function is called after a module has been stopped after a successful experiment run,
     * and the metadata for this module is stored together with the other experiment data.
     */
    virtual QVariantHash experimentMetadata();

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
     * @brief Called when one of this module's input ports is subscribed to a stream.
     */
    virtual void inputPortConnected(VarStreamInputPort *inPort);

    /**
     * @brief Return last error
     * @return The last error message generated by this module
     */
    QString lastError() const;

    void clearInPorts();
    void clearOutPorts();

    void removeInPortById(const QString &id);
    void removeOutPortById(const QString &id);

    QList<std::shared_ptr<VarStreamInputPort>> inPorts() const;
    QList<std::shared_ptr<StreamOutputPort>> outPorts() const;

    std::shared_ptr<VarStreamInputPort> inPortById(const QString &id) const;
    std::shared_ptr<StreamOutputPort> outPortById(const QString &id) const;

    QJsonValue serializeDisplayUiGeometry();
    void restoreDisplayUiGeometry(QJsonObject info);

    void setTimer(std::shared_ptr<SyncTimer> timer);

signals:
    void actionsUpdated();
    void stateChanged(ModuleState state);
    void error(const QString& message);
    void statusMessage(const QString& message);
    void nameChanged(const QString& name);
    void portsConnected(const VarStreamInputPort *inPort, const StreamOutputPort *outPort);
    void portConfigurationUpdated();
    void synchronizerDetailsChanged(const QString &id, const TimeSyncStrategies &strategies,
                                    const std::chrono::microseconds tolerance,
                                    const std::chrono::microseconds checkInterval);
    void synchronizerOffsetChanged(const QString &id, const std::chrono::microseconds currentOffset);

protected:
    void raiseError(const QString& message);

    QByteArray jsonObjectToBytes(const QJsonObject& object);
    QJsonObject jsonObjectFromBytes(const QByteArray& data);

    void setStatusMessage(const QString& message);
    bool makeDirectory(const QString &dir);

    /**
     * @brief Get name of a dataset from subscription metadata
     *
     * Some modules make a suggestion to their subscribers as for how they may want the
     * stored data to be named. This function extracts a suitable dataset name from
     * subscription metadata.
     *
     * This function always returns a valid name, if no suggestion
     * is given from the subscriber, the module's name is used.
     */
    QString datasetNameFromSubMetadata(const QVariantHash &subMetadata);

    /**
     * @brief Get file basename for data from subscription metadata
     *
     * Some modules make a suggestion to their subscribers as for how they may want the
     * stored data to be named. This function extracts a suitable file name from
     * the metadata, which can be used in a dataset.
     *
     * This function returns the "default" string if no suggestion was made.
     */
    QString dataBasenameFromSubMetadata(const QVariantHash &subMetadata, const QString &defaultName = QStringLiteral("data"));

    /**
     * @brief Create or retrieve default dataset for data storage in default storage group
     * @param preferredName The preferred name for the dataset.
     * @param subMetadata Metadata from a stream subscription, in case using the desired name from a module upstream is wanted.
     *
     * Retrieve the default dataset with the given name for this module. The module can use it to get file paths to write data to.
     * In case a module needs to create more then one dataset, creating a new group is advised first.
     * Use the createStorageGroup() and getOrCreateDatasetInGroup() methods for that.
     *
     * This method will return NULL in case the module was not yet assigned a data storage group. Use it only in and after the PREPARE
     * phase to get a valid dataset!
     */
    std::shared_ptr<EDLDataset> getOrCreateDefaultDataset(const QString &preferredName = QString(), const QVariantHash &subMetadata = QVariantHash());
    std::shared_ptr<EDLDataset> getOrCreateDatasetInGroup(std::shared_ptr<EDLGroup> group, const QString &preferredName = QString(), const QVariantHash &subMetadata = QVariantHash());

    /**
     * @brief Create new data storage group with the given name
     * @param groupName
     *
     * Creates a new data storage group name, or retrieves a reference to the group in case one with this name already exists.
     * The group is placed as child of the module's default storage group.
     *
     * This method will return NULL in case the module was not yet assigned a data storage group. Use it only in and after the PREPARE
     * phase to get a valid dataset!
     */
    std::shared_ptr<EDLGroup> createStorageGroup(const QString &groupName);

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

    /**
     * @brief Get new frequency/counter synchronizer
     *
     * This function can be called in the PREPARING phase of a module to retrieve a synchronizer
     * for devices which have a continuous counter and a sampling frequency.
     * A synchronizer can be used to semi-automatically synchronize a device timestamp or device clock
     * with the master time.
     * You will need to supply a fixed sampling rate in Hz for the given device, so the synchronizer can
     * determine a timestamp for a sample index.
     *
     * Returns: A new unique counter synchronizer, or NULL if we could not create one because no master timer existed.
     */
    std::unique_ptr<FreqCounterSynchronizer> initCounterSynchronizer(double frequencyHz);

    /**
     * @brief Get new secondary clock synchronizer
     *
     * This function can be called in the PREPARING phase of a module to retrieve a synchronizer
     * for devices which have an external clock to be synchronized and a known sampling frequency.
     * A synchronizer can be used to semi-automatically synchronize a device clock with the master timer.
     * You will need to supply a fixed sampling rate in Hz for the given device, so the synchronizer can
     * determine an expected timestamp to compare with the actually retrieved time.
     *
     * Returns: A new unique clock synchronizer, or NULL if we could not create one because no master timer existed.
     */
    std::unique_ptr<SecondaryClockSynchronizer> initClockSynchronizer(double frequencyHz);

    void setInitialized();
    bool initialized() const;

    std::atomic_bool m_running;

    std::shared_ptr<SyncTimer> m_syTimer;

private:
    Q_DISABLE_COPY(AbstractModule)
    class Private;
    QScopedPointer<Private> d;

    QMap<QString, std::shared_ptr<StreamOutputPort>> m_outPorts;
    QMap<QString, std::shared_ptr<VarStreamInputPort>> m_inPorts;

    void setState(ModuleState state);
    void setId(const QString &id);
    void setIndex(int index);
    void setStorageGroup(std::shared_ptr<EDLGroup> edlGroup);
};

} // end of namespace
#endif // MODULEAPI_H
