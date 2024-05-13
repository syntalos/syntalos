/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this software.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MODULEAPI_H
#define MODULEAPI_H

#include <QAction>
#include <QByteArray>
#include <QDebug>
#include <QList>
#include <QObject>
#include <QPixmap>

#include "modconfig.h"
#include "optionalwaitcondition.h"
#include "streams/stream.h"
#include "datactl/datatypes.h"
#include "datactl/edlstorage.h"
#include "datactl/syclock.h"
#include "datactl/timesync.h"

namespace Syntalos
{

class AbstractModule;
class StreamOutputPort;

/**
 * @brief The ModuleFeature flags
 * List of basic features this module may or may not support,
 * or may request from the engine to be available.
 */
enum class ModuleFeature {
    NONE = 0,
    SHOW_SETTINGS = 1 << 0, /// Module can display a settings window
    SHOW_DISPLAY = 1 << 1,  /// Module has one or more display window(s) to show
    SHOW_ACTIONS = 1 << 2,  /// Module supports context menu actions
    REALTIME = 1 << 3,      /// Enable realtime scheduling for the module's thread
    REQUEST_CPU_AFFINITY =
        1 << 4, /// Pin the module's thread to a separate CPU core, if possible (even if the user disabled this)
    PROHIBIT_CPU_AFFINITY =
        1 << 5,              /// Never set a core affinity for the thread of this module, even if the user wanted it
    CALL_UI_EVENTS = 1 << 6, /// Call direct UI events processing method
};
Q_DECLARE_FLAGS(ModuleFeatures, ModuleFeature)
Q_DECLARE_OPERATORS_FOR_FLAGS(ModuleFeatures)

/**
 * @brief The ModuleDriverKind enum
 */
enum class ModuleDriverKind {
    NONE,             /// Module will be run in the main (GUI) thread
    THREAD_DEDICATED, /// Module wants to run in a dedicated thread
    EVENTS_DEDICATED, /// Module shares a thread(pool) with other modules of its kind, via an event loop
    EVENTS_SHARED     /// Module shares a thread(pool) with arbitrary other modules, actions are triggered by events
};

/**
 * @brief The UsbHotplugEventKind enum
 */
enum class UsbHotplugEventKind {
    NONE,           /// No USB hotplug event
    DEVICE_ARRIVED, /// A new device appeared
    DEVICE_LEFT,    /// A device has left
    DEVICES_CHANGE  /// Devices have appeared and left
};

/**
 * @brief Categorization for modules
 */
enum class ModuleCategory : uint32_t {
    NONE = 0,              /// Not categorized
    SYNTALOS_DEV = 1 << 0, /// A development/test tool for Syntalos itself
    EXAMPLE = 1 << 1,      /// An example / template module
    DEVICE = 1 << 2,       /// Modules which communicate with hwrdware devices
    GENERATOR = 1 << 3,    /// (Test)data generators)
    SCRIPTING = 1 << 4,    /// Scripting & customization
    DISPLAY = 1 << 5,      /// Display modules
    WRITERS = 1 << 6,      /// Modules which write data to disk
    PROCESSING = 1 << 7    /// Data processing modules
};
Q_DECLARE_FLAGS(ModuleCategories, ModuleCategory)
Q_DECLARE_OPERATORS_FOR_FLAGS(ModuleCategories)

QString toString(ModuleCategory category);
ModuleCategory moduleCategoryFromString(const QString &categoryStr);
ModuleCategories moduleCategoriesFromString(const QString &categoriesStr);

/**
 * @brief Static information about a module
 */
class Q_DECL_EXPORT ModuleInfo
{
    friend class Engine;

public:
    explicit ModuleInfo();
    virtual ~ModuleInfo();

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
    virtual QIcon icon() const;

    /**
     * @brief Called when the icon should be reloaded or updated.
     */
    virtual void refreshIcon();

    /**
     * @brief The dominant color for this module.
     */
    virtual QColor color() const;

    /**
     * @brief Categories this module belongs to
     */
    virtual ModuleCategories categories() const;

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

    QString rootDir() const;
    void setRootDir(const QString &dir);

protected:
    void setIcon(const QIcon &icon);

private:
    Q_DISABLE_COPY(ModuleInfo)
    class Private;
    QScopedPointer<Private> d;

    void setCount(int count);
};

/**
 * Define interfaces for a dynamically loaded Syntalos module, so we can
 * find it at runtime.
 */
#define SYNTALOS_DECLARE_MODULE                                                            \
    _Pragma("GCC visibility push(default)") extern "C" ModuleInfo *syntalos_module_info(); \
    extern "C" const char *syntalos_module_api_id();                                       \
    _Pragma("GCC visibility pop")

/**
 * Define interfaces for a dynamically loaded Syntalos module, so we can
 * find it at runtime.
 */
#define SYNTALOS_MODULE(MI)              \
    ModuleInfo *syntalos_module_info()   \
    {                                    \
        return new MI##Info;             \
    }                                    \
    const char *syntalos_module_api_id() \
    {                                    \
        return SY_MODULE_API_TAG;        \
    }

/**
 * @brief The TestSubject struct
 * Data about a test subject.
 */
struct Q_DECL_EXPORT TestSubject {
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

class Q_DECL_EXPORT AbstractStreamPort
{
public:
    virtual ~AbstractStreamPort() = default;

    virtual QString id() const = 0;
    virtual QString title() const = 0;

    virtual PortDirection direction() const
    {
        return PortDirection::NONE;
    }

    virtual int dataTypeId() const = 0;
    virtual QString dataTypeName() const = 0;

    virtual AbstractModule *owner() const = 0;
};

class Q_DECL_EXPORT VarStreamInputPort : public AbstractStreamPort
{
public:
    explicit VarStreamInputPort(AbstractModule *owner, const QString &id, const QString &title);
    virtual ~VarStreamInputPort();

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
        m_acceptedTypeId = syDataTypeId<T>();
        m_acceptedTypeName = BaseDataType::typeIdToString(m_acceptedTypeId);
    }

    std::shared_ptr<StreamSubscription<T>> subscription()
    {
        auto sub = std::dynamic_pointer_cast<StreamSubscription<T>>(m_sub.value());
        if (sub == nullptr) {
            if (hasSubscription()) {
                qCritical().noquote() << "Conversion of variant subscription to dedicated type" << typeid(T).name()
                                      << "failed."
                                      << "Modules are connected in a way they shouldn't be (terminating now).";
                qFatal("Bad module connection.");
                assert(0);
            } else {
                qWarning().noquote() << "Tried to obtain" << typeid(T).name()
                                     << "subscription from a port that was not subscribed to anything.";
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

class Q_DECL_EXPORT StreamOutputPort : public AbstractStreamPort
{
public:
    explicit StreamOutputPort(
        AbstractModule *owner,
        const QString &id,
        const QString &title,
        std::shared_ptr<VariantDataStream> stream);
    virtual ~StreamOutputPort();

    bool canSubscribe(const QString &typeName);
    int dataTypeId() const override;
    QString dataTypeName() const override;

    template<typename T>
    std::shared_ptr<DataStream<T>> stream()
    {
        return std::dynamic_pointer_cast<DataStream<T>>(streamVar());
    }
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

/**
 * @brief Create a new DataStream for the type identified by the given ID.
 */
VariantDataStream *newStreamForType(int typeId);

/**
 * @brief Create a new Input Port for the type identified by the given ID.
 */
VarStreamInputPort *newInputPortForType(int typeId, AbstractModule *mod, const QString &id, const QString &title);

/// Event function type for timed callbacks
using intervalEventFunc_t = void (AbstractModule::*)(int &);

/// Event function type for subscription new data callbacks
using recvDataEventFunc_t = void (AbstractModule::*)();

/**
 * @brief Abstract base class for all modules
 *
 * This class describes the interface for every Syntalos module and contains
 * helper functions and other API to make writing modules simple.
 */
class Q_DECL_EXPORT AbstractModule : public QObject
{
    Q_OBJECT
    friend class Engine;

public:
    explicit AbstractModule(QObject *parent = nullptr);
    explicit AbstractModule(const QString &id, QObject *parent = nullptr);
    virtual ~AbstractModule();

    ModuleState state() const;

    /**
     * @brief Allow a running module to mark itself as idle.
     *
     * This may be useful in case the module us missing an essential stream
     * connection and essentially does nothing in a run.
     */
    void setStateDormant();

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
    virtual void setName(const QString &name);

    /**
     * @brief Select how this module will be executed by the Syntalos engine.
     */
    virtual ModuleDriverKind driver() const;

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
        requires std::is_base_of_v<BaseDataType, T>
    std::shared_ptr<DataStream<T>> registerOutputPort(const QString &id, const QString &title = QString())
    {
        if (m_outPorts.contains(id)) {
            qWarning().noquote() << "Module" << name() << "already registered an output port with ID:" << id;
            return m_outPorts[id]->stream<T>();
        }

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
        requires std::is_base_of_v<BaseDataType, T>
    std::shared_ptr<StreamInputPort<T>> registerInputPort(const QString &id, const QString &title = QString())
    {
        if (m_inPorts.contains(id)) {
            qWarning().noquote() << "Module" << name() << "already registered an input port with ID:" << id;
            return std::dynamic_pointer_cast<StreamInputPort<T>>(m_inPorts[id]);
        }

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
    std::shared_ptr<VariantDataStream> registerOutputPortByTypeId(
        int typeId,
        const QString &id,
        const QString &title = QString())
    {
        if (m_outPorts.contains(id)) {
            qWarning().noquote() << "Module" << name() << "already registered an output port with ID:" << id;
            return m_outPorts[id]->streamVar();
        }

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
    std::shared_ptr<VarStreamInputPort> registerInputPortByTypeId(
        int typeId,
        const QString &id,
        const QString &title = QString())
    {
        if (m_inPorts.contains(id)) {
            qWarning().noquote() << "Module" << name() << "already registered an input port with ID:" << id;
            return m_inPorts[id];
        }

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
    virtual bool prepare(const TestSubject &testSubject) = 0;

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
     * @brief Called to process UI events
     *
     * If ModuleFeature::CALL_UI_EVENTS is set, the engine will explicitly
     * call this method in quick succession to process UI events.
     * Usually modules can hook into the main UI event loop using a QTimer
     * at interval 0, but the added overhead of this method may be significant
     * for modules which have to draw a lot of things on screen in quick succession.
     * These modules may opt to implement this modules and set the CALL_UI_EVENTS
     * feature for more efficient continuous drawing callbacks.
     */
    virtual void processUiEvents();

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
    virtual QList<QAction *> actions();

    /**
     * @brief Serialize the settings of this module
     *
     * Modules can add their settings keys and values to the Variant hashtable,
     * and also can store arbitrary data as a byte array if they so choose.
     */
    virtual void serializeSettings(const QString &confBaseDir, QVariantHash &settings, QByteArray &extraData);

    /**
     * @brief Load settings from previously stored data.
     *
     * See serializeSettings() for details. This method should act in reverse and restore
     * settings from previously saved data.
     * @return true if successful.
     */
    virtual bool loadSettings(const QString &confBaseDir, const QVariantHash &settings, const QByteArray &extraData);

    /**
     * @brief Called when one of this module's input ports is subscribed to a stream.
     */
    virtual void inputPortConnected(VarStreamInputPort *inPort);

    /**
     * @brief Update thread start wait condition before each run.
     *
     * This is a convenience funtion to provide modules which roll their own threads with a
     * start-wait-condition just like Syntalos-managed threads have. The condition is only
     * valid for the current run, and invalid afterwards!
     */
    virtual void updateStartWaitCondition(OptionalWaitCondition *waitCondition);

    /**
     * @brief Return last error
     * @return The last error message generated by this module
     */
    QString lastError() const;

    /**
     * @brief Obtain the root directory of this (loadable) module.
     * @return Directory of this module as absolute path.
     */
    QString moduleRootDir() const;

    /**
     * @brief Set maximum modules per thread when using dedicated event driver
     *
     * When this module's driver is ModuleDriverKind::EVENTS_DEDICATED, the module can set
     * a maximum amount of its instances that should share a thread.
     * This setting is shared between all instances of this module, the last module to change
     * it "wins".
     * Setting the value to -1 or 0 (the default) will result in an unlimited amount of instances
     * sharing a single thread.
     */
    void setEventsMaxModulesPerThread(int maxModuleCount);
    int eventsMaxModulesPerThread() const;

    void clearInPorts();
    void clearOutPorts();

    void removeInPortById(const QString &id);
    void removeOutPortById(const QString &id);

    QList<std::shared_ptr<VarStreamInputPort>> inPorts() const;
    QList<std::shared_ptr<StreamOutputPort>> outPorts() const;

    std::shared_ptr<VarStreamInputPort> inPortById(const QString &id) const;
    std::shared_ptr<StreamOutputPort> outPortById(const QString &id) const;

    QList<QPair<intervalEventFunc_t, int>> intervalEventCallbacks() const;
    QList<QPair<recvDataEventFunc_t, std::shared_ptr<VariantStreamSubscription>>> recvDataEventCallbacks() const;

    QVariant serializeDisplayUiGeometry();
    void restoreDisplayUiGeometry(const QVariant &var);

    void setTimer(std::shared_ptr<SyncTimer> timer);

Q_SIGNALS:
    void actionsUpdated();
    void stateChanged(ModuleState state);
    void error(const QString &message);
    void statusMessage(const QString &message);
    void nameChanged(const QString &name);
    void portsConnected(const VarStreamInputPort *inPort, const StreamOutputPort *outPort);
    void portConfigurationUpdated();
    void synchronizerDetailsChanged(
        const QString &id,
        const Syntalos::TimeSyncStrategies &strategies,
        const Syntalos::microseconds_t &tolerance);
    void synchronizerOffsetChanged(const QString &id, const Syntalos::microseconds_t &currentOffset);

protected:
    void raiseError(const QString &message);

    void setStatusMessage(const QString &message);
    bool makeDirectory(const QString &dir);
    void appProcessEvents();

    /**
     * @brief Suggested name for datasets from this module
     *
     * Creates a good name based on the modules human-chosen name for
     * datasets generated by it. This includes removing special characters,
     * like slashes, from the name and simplifying the name string in general.
     *
     * Modules should use this function to choose a dataset name, but they may
     * also decide to use a different, custom name.
     */
    QString datasetNameSuggestion(bool lowercase = true) const;

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
    QString datasetNameFromSubMetadata(const QVariantHash &subMetadata) const;

    /**
     * @brief Get name of a dataset from subscription metadata and preferred name
     *
     * This functions acts like datasetNameFromSubMetadata(), but will also
     * try to use the supplied preferred name if possible.
     *
     * This function always returns a valid name, if no suggestion
     * is given from the subscriber, the module's name is used.
     */
    QString datasetNameFromParameters(const QString &preferredName, const QVariantHash &subMetadata) const;

    /**
     * @brief Get file basename for data from subscription metadata
     *
     * Some modules make a suggestion to their subscribers as for how they may want the
     * stored data to be named. This function extracts a suitable file name from
     * the metadata, which can be used in a dataset.
     *
     * This function returns the "default" string if no suggestion was made.
     */
    QString dataBasenameFromSubMetadata(
        const QVariantHash &subMetadata,
        const QString &defaultName = QStringLiteral("data"));

    /**
     * @brief Create dataset for data storage in default storage group for this module
     * @param preferredName The preferred name for the dataset.
     * @param subMetadata Metadata from a stream subscription, in case using the desired name from a module upstream is
     * wanted.
     *
     * Create the default dataset with the given name for this module. The module can use it to get file paths to
     * write data to. In case a module needs to create more then one dataset, creating a new group is advised first. Use
     * the createStorageGroup() and createDatasetInGroup() methods for that.
     *
     * This method will return NULL in case the module was not yet assigned a data storage group. Use it only in and
     * after the PREPARE phase to get a valid dataset!
     *
     * An error will be emitted if the requested dataset has already existed before, an NULL is returned in that case
     * as well. This design exists to prevent two modules from trying to write to the same file.
     */
    std::shared_ptr<EDLDataset> createDefaultDataset(
        const QString &preferredName = QString(),
        const QVariantHash &subMetadata = QVariantHash());
    std::shared_ptr<EDLDataset> createDatasetInGroup(
        std::shared_ptr<EDLGroup> group,
        const QString &preferredName = QString(),
        const QVariantHash &subMetadata = QVariantHash());

    /**
     * @brief Obtain the module's default dataset if it exists
     *
     * If a default dataset has been created using createDefaultDataset(), return
     * it. Otherwise return NULL.
     */
    std::shared_ptr<EDLDataset> getDefaultDataset();
    std::shared_ptr<EDLDataset> getDatasetInGroup(
        std::shared_ptr<EDLGroup> group,
        const QString &preferredName = QString(),
        const QVariantHash &subMetadata = QVariantHash());

    /**
     * @brief Create new data storage group with the given name
     * @param groupName
     *
     * Creates a new data storage group name, or retrieves a reference to the group in case one with this name already
     * exists. The group is placed as child of the module's default storage group.
     *
     * This method will return NULL in case the module was not yet assigned a data storage group. Use it only in and
     * after the PREPARE phase to get a valid dataset!
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
    QWidget *addDisplayWindow(QWidget *window, bool owned = true);

    /**
     * @brief Register a settings window for this module.
     * @param window
     * @param owned
     *
     * Add a new settings window to this module.
     * The window instance will be deleted on destruction, if
     * #own was set to true.
     */
    QWidget *addSettingsWindow(QWidget *window, bool owned = true);

    /**
     * @brief Request a member function of this module to be called at an interval
     *
     * Set a pointer to a member function of this module as first paremter, to be called
     * at a interval set as second parameter in milliseconds.
     * If the interval selected is 0, the function will be called as soon as possible.
     *
     * The first parameter of the callback is a reference to the execution interval, which
     * the callee may adjust to be run less or more frequent. Adjusting the frequency does
     * not come at zero cost, so please avoid very frequent adjustments.
     *
     * Since these functions are scheduled together with other possible events in an event
     * loop, do not expect the member function to be called in exactly the requested intervals.
     * The interval will also not be adjusted to "catch up" for lost time.
     *
     * Please ensure that the callback function never blocks for an extended period of time
     * to give other modules a chance to run as well. Also, you can expect this function to
     * be run in a different thread compared to where the module's prepare() function was run.
     * The function may even move between threads, so make sure it is reentrant.
     */
    template<typename T>
    void registerTimedEvent(void (T::*fn)(int &), const milliseconds_t &interval)
    {
        static_assert(
            std::is_base_of<AbstractModule, T>::value,
            "Callback needs to point to a member function of a class derived from AbstractModule");
        const auto amFn = static_cast<intervalEventFunc_t>(fn);
        m_intervalEventCBList.append(qMakePair(amFn, interval.count()));
    }

    /**
     * @brief Request a member function of this module to be called when a subscription has new data.
     *
     * Set a pointer to a member function of this module as first parameter, to be called
     * once the stream subscription given as second parameter has received more data.
     *
     * Please ensure that the callback function never blocks for an extended period of time
     * to give other modules a chance to run as well. Also, you can expect this function to
     * be run in a different thread compared to where the module's prepare() function was run.
     * The function may even move between threads, so make sure it is reentrant.
     */
    template<typename T>
    void registerDataReceivedEvent(void (T::*fn)(), std::shared_ptr<VariantStreamSubscription> subscription)
    {
        static_assert(
            std::is_base_of<AbstractModule, T>::value,
            "Callback needs to point to a member function of a class derived from AbstractModule");
        const auto amFn = static_cast<recvDataEventFunc_t>(fn);
        m_recvDataEventCBList.append(qMakePair(amFn, subscription));
    }

    /**
     * @brief Remove all registered data-received event callbacks.
     */
    void clearDataReceivedEventRegistrations();

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
     * The frequency of the secondary clock set here determines how many datapoints the synchronizer will store
     * to calculate the divergence of the secondary clock time from master time. If set to zero, a fixed amount
     * of timepoints will be used instead.
     *
     * Since this synchronizer will only look at the difference between master and secondary clock time per measurement,
     * the secondary clock may change its speed (as is the case with many cameras that don't run at an exactly constant
     * framerate but sometimes produce frames faster or slower than before)
     *
     * Returns: A new unique clock synchronizer, or NULL if we could not create one because no master timer existed.
     */
    std::unique_ptr<SecondaryClockSynchronizer> initClockSynchronizer(double expectedFrequencyHz = 0);

    /**
     * @brief Potential amount of CPUs not used by other syntalos tasks.
     *
     * This is a rough guess and may not at all reflect reality. Modules may be interested
     * in this information regardless, to scale the amount of external threads they themselves may
     * create without control of Syntalos (in this event, it may be beneficial to give the user some
     * control over this behavior, as it is impossible to know how heavy the workload of other modules
     * on other threads actually is).
     */
    uint potentialNoaffinityCPUCount() const;

    /**
     * @brief Default thread realtime priority
     *
     * The default priority for realtime threads Syntalos is supposed to use.
     * This value can be used if the module needs to manage its own realtime threads
     * and wants to set the default priority on them (but managing threads on their own
     * is generally discouraged for modules).
     *
     * @return RT thread priority value
     */
    int defaultRealtimePriority() const;

    /**
     * @brief Returns true if the currently ongoing or last run is/was ephemeral
     *
     * An emphameral (or volatile) run is a type of run where no data is stored permanently.
     * Usually, a Syntalos module will not need to handle this type of run explicitly, but there
     * are some cases where a module needs to be aware that all data will get deleted immediately
     * after a run has completed (e.g. if postprocessing steps are deferred to an external process).
     * @return True if the current run is/was ephemeral.
     */
    bool isEphemeralRun() const;

    /**
     * @brief Handle USB hotplug events
     *
     * This function is called by the engine when a new USB device appears or leaves.
     * Its call is deferred when an experiment is running and dispatched after a run
     * has completed.
     */
    virtual void usbHotplugEvent(UsbHotplugEventKind kind);

    void setInitialized();
    bool initialized() const;

    std::atomic_bool m_running;

    /**
     * @brief Global master timer reference for this module, valid only during a run.
     */
    std::shared_ptr<SyncTimer> m_syTimer;

private:
    Q_DISABLE_COPY(AbstractModule)
    class Private;
    QScopedPointer<Private> d;

    QMap<QString, std::shared_ptr<StreamOutputPort>> m_outPorts;
    QMap<QString, std::shared_ptr<VarStreamInputPort>> m_inPorts;

    QList<QPair<intervalEventFunc_t, int>> m_intervalEventCBList;
    QList<QPair<recvDataEventFunc_t, std::shared_ptr<VariantStreamSubscription>>> m_recvDataEventCBList;

    void setState(ModuleState state);
    void setId(const QString &id);
    void setIndex(int index);
    void setSimpleStorageNames(bool enabled);
    void setStorageGroup(std::shared_ptr<EDLGroup> edlGroup);
    void resetEventCallbacks();
    void setPotentialNoaffinityCPUCount(uint coreN);
    void setDefaultRTPriority(int prio);
    void setEphemeralRun(bool isEphemeral);
};

} // namespace Syntalos

#endif // MODULEAPI_H
