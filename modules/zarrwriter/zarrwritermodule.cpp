/*
 * Copyright (C) 2025-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "zarrwritermodule.h"
#include "zarrv3writer.h"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QDialogButtonBox>
#include <QUuid>

#include <Eigen/Core>

#include "datatypeselector.h"

SYNTALOS_MODULE(ZarrWriterModule)

/**
 * Zarr data type matching a signal block's scalar sample type.
 */
template<typename Scalar>
constexpr ZarrV3Array::DType zarrDTypeFor()
{
    if constexpr (std::is_same_v<Scalar, int16_t>)
        return ZarrV3Array::DType::Int16;
    else if constexpr (std::is_same_v<Scalar, uint16_t>)
        return ZarrV3Array::DType::UInt16;
    else if constexpr (std::is_same_v<Scalar, int32_t>)
        return ZarrV3Array::DType::Int32;
    else if constexpr (std::is_same_v<Scalar, uint32_t>)
        return ZarrV3Array::DType::UInt32;
    else if constexpr (std::is_same_v<Scalar, uint64_t>)
        return ZarrV3Array::DType::UInt64;
    else if constexpr (std::is_same_v<Scalar, float>)
        return ZarrV3Array::DType::Float32;
    else if constexpr (std::is_same_v<Scalar, double>)
        return ZarrV3Array::DType::Float64;
    else
        static_assert(sizeof(Scalar) == 0, "No Zarr data type known for this signal block scalar type");
}

/**
 * Port ID / title for an input of the given stream type.
 * The IDs are persisted in project files, so they must remain stable.
 */
static std::pair<QString, QString> inputPortNamesForType(int typeId)
{
    if (typeId == LineReading::staticTypeId())
        return {QStringLiteral("lines-in"), QStringLiteral("Line Readings")};

    std::pair<QString, QString> names;
    withSignalBlockType(typeId, [&](auto tag) {
        using Scalar = signal_block_scalar_t<typename decltype(tag)::type>;
        names.first = QString::fromStdString(scalarTypeName<Scalar>(true) + "sig-in");
        names.second = QString::fromStdString(scalarTypeName<Scalar>() + " Signals");
    });
    return names;
}

class ZarrSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ZarrSettingsDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(QStringLiteral("Zarr Writer Settings"));
        setMinimumWidth(360);

        auto layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);

        auto sourceGroup = new QGroupBox(QStringLiteral("Data Source"), this);
        auto sourceLayout = new QFormLayout(sourceGroup);
        sourceLayout->setContentsMargins(4, 4, 4, 4);

        m_sourceSel = new DataTypeSelector(this);
        m_sourceSel->addNoneEntry();
        m_sourceSel->addSignalBlockTypes();
        m_sourceSel->addDataType(LineReading::staticTypeId(), QStringLiteral("Line Readings"));
        sourceLayout->addRow(QStringLiteral("Input type:"), m_sourceSel);

        layout->addWidget(sourceGroup);

        connect(m_sourceSel, &DataTypeSelector::selectionChanged, this, &ZarrSettingsDialog::settingsChanged);

        auto nameGroup = new QGroupBox(QStringLiteral("Dataset Name"), this);
        auto nameLayout = new QFormLayout(nameGroup);
        nameLayout->setContentsMargins(4, 4, 4, 4);

        m_cbNameFromSrc = new QCheckBox(QStringLiteral("Use name from data source"), this);
        m_cbNameFromSrc->setChecked(true);
        nameLayout->addRow(m_cbNameFromSrc);

        m_nameEdit = new QLineEdit(this);
        m_nameEdit->setEnabled(false);
        m_nameEdit->setPlaceholderText(QStringLiteral("e.g. my-signals"));
        nameLayout->addRow(QStringLiteral("Dataset name:"), m_nameEdit);

        layout->addWidget(nameGroup);
        layout->addStretch();

        auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, Qt::Horizontal, this);
        layout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
        connect(m_cbNameFromSrc, &QCheckBox::toggled, this, [this](bool checked) {
            m_nameEdit->setEnabled(!checked);
        });
    }

    void setRunning(bool running)
    {
        // The port topology must not change during a run
        m_sourceSel->setEnabled(!running);
        m_cbNameFromSrc->setEnabled(!running);
        m_nameEdit->setEnabled(!running && !m_cbNameFromSrc->isChecked());
    }

    int selectedTypeId() const
    {
        return m_sourceSel->selectedTypeId();
    }
    QString selectedTypeName() const
    {
        return m_sourceSel->selectedTypeName();
    }
    void setSelectedTypeName(const QString &typeName)
    {
        m_sourceSel->setSelectedTypeName(typeName);
    }

    bool useNameFromSource() const
    {
        return m_cbNameFromSrc->isChecked();
    }
    void setUseNameFromSource(bool fromSource)
    {
        m_cbNameFromSrc->setChecked(fromSource);
        m_nameEdit->setEnabled(!fromSource);
    }

    QString dataName() const
    {
        return m_nameEdit->text().trimmed();
    }
    void setDataName(const QString &name)
    {
        m_nameEdit->setText(name);
    }

Q_SIGNALS:
    void settingsChanged();

private:
    DataTypeSelector *m_sourceSel;
    QCheckBox *m_cbNameFromSrc;
    QLineEdit *m_nameEdit;
};

// Chunk limits, see chunkCountFromSampleRate() for details
static constexpr int64_t ZARR_CHUNK_MIN = 1000;
static constexpr int64_t ZARR_CHUNK_MAX = 100000;
static constexpr int64_t ZARR_CHUNK_DEFAULT = 10000; // fallback when sample rate is unknown

class ZarrWriterModule : public AbstractModule
{
    Q_OBJECT

private:
    // Exactly one input port exists at a time, of the type the user selected.
    // Its concrete stream type is only looked at when data arrives.
    std::shared_ptr<VarStreamInputPort> m_inPort;

    // the subscription we read from in the current run, to look at its metadata
    std::shared_ptr<VariantStreamSubscription> m_sub;
    bool m_lineEvents; // input is a sparse LineReading event stream, not signal blocks
    bool m_writeData;

    ZarrSettingsDialog *m_settingsDlg;

public:
    explicit ZarrWriterModule(ZarrWriterModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent),
          m_lineEvents(false),
          m_writeData(false)
    {
        m_settingsDlg = new ZarrSettingsDialog();
        m_settingsDlg->setWindowIcon(modInfo->icon());
        addSettingsWindow(m_settingsDlg);

        connect(m_settingsDlg, &ZarrSettingsDialog::settingsChanged, this, [this]() {
            updatePortConfiguration();
        });
    }

    std::expected<void, QString> initialize() override
    {
        // start with no input ports; the user picks exactly one type in the settings
        updatePortConfiguration();
        return {};
    }

    void updatePortConfiguration()
    {
        // Rebuild the single input port to match the selected data type.
        // Only safe on the main thread while not running.
        clearInPorts();
        m_inPort.reset();

        setStatusMessage({});
        const int typeId = m_settingsDlg->selectedTypeId();
        const auto [portId, portTitle] = inputPortNamesForType(typeId);
        if (portId.isEmpty()) {
            setStatusMessage("No input port type selected!");
            return;
        }
        m_inPort = registerInputPortByTypeId(typeId, portId, portTitle);
    }

    ~ZarrWriterModule() override = default;

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_SETTINGS;
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::EVENTS_DEDICATED;
    }

    bool prepare(const RunInfo &info) override
    {
        m_settingsDlg->setRunning(true);

        if (!m_settingsDlg->useNameFromSource() && m_settingsDlg->dataName().isEmpty()) {
            raiseError(QStringLiteral("Dataset name is not set. Please set it in the settings to continue."));
            return false;
        }

        m_writeData = !info.isEphemeral;

        m_sub.reset();
        if (!m_inPort || !m_inPort->hasSubscription()) {
            // nothing is connected, so there is nothing for us to do this run
            setStateDormant();
            return true;
        }

        m_sub = m_inPort->subscriptionVar();
        m_lineEvents = m_sub->dataTypeId() == LineReading::staticTypeId();

        setWorker(
            Worker{
                .sub = m_sub,
                .lineEvents = m_lineEvents,
                // we don't write anything to disk if we aren't going to use the data anyway
                .writeData = m_writeData,
            });

        setStateReady();
        return true;
    }

    void start() override
    {
        if (!m_sub || !m_writeData)
            return;

        // if we fail to set up the store, the worker must not try to write to it
        const auto disableWriting = [this] {
            modifyWorker<Worker>([](Worker &w) {
                w.writeData = false;
            });
        };

        // collect stream metadata
        const MetaStringMap mdata = m_sub->metadata();
        QStringList signalNames;
        for (const auto &v : m_sub->metadataValue("signal_names", MetaArray{}))
            if (const auto s = v.get<std::string>())
                signalNames << QString::fromStdString(*s);
        const auto dataUnit = QString::fromStdString(m_sub->metadataValue("data_unit", std::string{}));
        QString timeUnit;
        double dataScale = 1.0;
        double dataOffset = 0.0;
        double sampleRate = -1.0;
        if (m_lineEvents) {
            // Sparse edge events: recorded as a timestamps array + a 2-column
            // [line_id, value] data array. The data columns are fixed, so the
            // upstream signal_names (line labels) are kept only for the dataset
            // attributes, not the array schema. Events are irregular, so the
            // default chunk size is used.
            timeUnit = QString::fromStdString(m_sub->metadataValue("time_unit", std::string{"microseconds"}));
        } else {
            timeUnit = QString::fromStdString(m_sub->metadataValue("time_unit", std::string{}));
            dataScale = m_sub->metadataValue("data_scale", 1.0);
            dataOffset = m_sub->metadataValue("data_offset", 0.0);
            sampleRate = m_sub->metadataValue("sample_rate", -1.0);
        }

        const auto srcModType = QString::fromStdString(mdata.valueOr<std::string>("src_mod_type", std::string{}));

        // create EDL dataset for this recording
        std::shared_ptr<EDLDataset> dset;
        if (m_settingsDlg->useNameFromSource())
            dset = createDefaultDataset(name(), mdata);
        else
            dset = createDefaultDataset(m_settingsDlg->dataName());

        if (!dset) {
            disableWriting();
            return;
        }

        // Mirror the signal metadata into the dataset's attributes.toml so the EDL
        // manifest is self-describing without parsing the Zarr store's zarr.json.
        if (sampleRate > 0 || timeUnit == "index")
            dset->insertAttribute("sample_rate", sampleRate);
        if (!timeUnit.isEmpty())
            dset->insertAttribute("time_unit", timeUnit.toStdString());
        if (!dataUnit.isEmpty())
            dset->insertAttribute("data_unit", dataUnit.toStdString());
        if (dataScale != 1.0)
            dset->insertAttribute("data_scale", dataScale);
        if (dataOffset != 0.0)
            dset->insertAttribute("data_offset", dataOffset);
        if (!srcModType.isEmpty())
            dset->insertAttribute("src_mod_type", srcModType.toStdString());
        if (!signalNames.isEmpty()) {
            MetaArray names;
            for (const auto &n : signalNames)
                names.push_back(n.toStdString());
            dset->insertAttribute("signal_names", names);
        }

        // register the Zarr store directory as the dataset's primary file
        auto storeName = dataBasenameFromSubMetadata(mdata, dset->name());
        storeName += ".zarr";
        const std::string storePath = dset->setDataFile(storeName);

        // Write Zarr v3 root group metadata
        if (!zarrWriteRootGroupMetadata(storePath)) {
            raiseError(QStringLiteral("Failed to create Zarr store directory or root metadata"));
            disableWriting();
            return;
        }

        // our worker is waiting for the run to start, give it the store to write to.
        // The arrays are created lazily on the first data block. We store the
        // expected channel count from metadata now so we can validate the
        // actual incoming data against it.
        modifyWorker<Worker>([&](Worker &w) {
            w.currentDSet = dset;
            w.storePath = storePath;
            w.signalNames = signalNames;
            w.timeUnit = timeUnit;
            w.dataUnit = dataUnit;
            w.dataScale = dataScale;
            w.dataOffset = dataOffset;
            w.sampleRate = sampleRate;
            w.expectedChannels = static_cast<int>(signalNames.size()); // 0 = not advertised
            w.chunkCount = chunkCountFromSampleRate(sampleRate);
        });
    }

    /**
     * Writes the received data into a Zarr store, in the event loop the module is assigned to.
     */
    struct Worker {
        WorkerContext mod{};
        std::shared_ptr<VariantStreamSubscription> sub;
        bool lineEvents; // input is a sparse LineReading event stream, not signal blocks
        bool writeData;

        // The store is only set up when the run is started, as it depends on metadata of the data source
        std::shared_ptr<EDLDataset> currentDSet{};
        std::string storePath{};

        QStringList signalNames{};
        QString timeUnit{};
        QString dataUnit{};
        double dataScale = 1.0;
        double dataOffset = 0.0;
        double sampleRate = -1.0;
        int expectedChannels = 0; // 0 = not advertised by upstream, the first block decides
        int64_t chunkCount = ZARR_CHUNK_DEFAULT;

        qint64 itemsWritten = 0;
        std::unique_ptr<ZarrV3Array> tsArray{};
        std::unique_ptr<ZarrV3Array> dataArray{};

        void setup(WorkerEvents &ev)
        {
            ev.onData(sub, [this] {
                onDataReceived();
            });
        }

        void ensureArraysInitialized(int nCols, ZarrV3Array::DType dataDtype)
        {
            // validate channel count against what the upstream source advertised or has sent before
            if (expectedChannels > 0 && nCols != expectedChannels) {
                mod.raiseError(QStringLiteral("Channel count mismatch: expected %1 channel(s) but received %2")
                                   .arg(expectedChannels)
                                   .arg(nCols));
                writeData = false;
                return;
            }

            // skip if we are already initialized
            if (tsArray)
                return;

            // 1-D timestamps array: shape = [total_samples], dtype = uint64
            tsArray = std::make_unique<ZarrV3Array>(
                QString::fromStdString(storePath),
                QStringLiteral("timestamps"),
                ZarrV3Array::DType::UInt64,
                chunkCount,
                1,
                QStringList{QStringLiteral("time")});

            if (!timeUnit.isEmpty()) {
                QJsonObject tsAttrs;
                tsAttrs["time_unit"] = timeUnit;
                tsArray->setAttributes(tsAttrs);
            }

            if (auto res = tsArray->open(); !res) {
                mod.raiseError(QStringLiteral("Failed to open timestamps array: ") + res.error());
                tsArray.reset();
                writeData = false;
                return;
            }

            // 2-D data array: shape = [total_samples, n_channels]
            const QStringList dataDimNames = {QStringLiteral("time"), QStringLiteral("channel")};
            dataArray = std::make_unique<ZarrV3Array>(
                QString::fromStdString(storePath),
                QStringLiteral("data"),
                dataDtype,
                chunkCount,
                nCols,
                dataDimNames);

            // embed signal metadata as Zarr array attributes. time_unit lives on
            // the timestamps array (which it describes), not here.
            QJsonObject dataAttrs;
            if (!signalNames.isEmpty()) {
                QJsonArray names;
                for (const auto &n : signalNames)
                    names.append(n);
                dataAttrs["signal_names"] = names;
            }
            if (!dataUnit.isEmpty())
                dataAttrs["data_unit"] = dataUnit;
            if (dataScale != 1.0)
                dataAttrs["data_scale"] = dataScale;
            if (dataOffset != 0.0)
                dataAttrs["data_offset"] = dataOffset;
            if (sampleRate > 0 || timeUnit == "index")
                dataAttrs["sample_rate"] = sampleRate;
            if (currentDSet)
                dataAttrs["collection_id"] = QString::fromStdString(currentDSet->collectionId().toHex());
            if (!dataAttrs.isEmpty())
                dataArray->setAttributes(dataAttrs);

            if (auto res = dataArray->open(); !res) {
                mod.raiseError(QStringLiteral("Failed to open data array: ") + res.error());
                tsArray.reset();
                dataArray.reset();
                writeData = false;
                return;
            }

            // the data array has this many columns now, so every block that follows must have them too
            expectedChannels = nCols;
        }

        /**
         * Lazily create the arrays for a LineReading event stream: a 1-D
         * `timestamps` array plus a 2-column `data` array holding [line_id, value]
         * per event. Row i of `data` pairs with timestamps[i]; both are appended one
         * row per event in lockstep so the triplet stays aligned.
         */
        void ensureLineArraysInitialized()
        {
            if (tsArray)
                return;

            tsArray = std::make_unique<ZarrV3Array>(
                QString::fromStdString(storePath),
                QStringLiteral("timestamps"),
                ZarrV3Array::DType::UInt64,
                chunkCount,
                1,
                QStringList{QStringLiteral("event")});
            if (!timeUnit.isEmpty()) {
                QJsonObject tsAttrs;
                tsAttrs["time_unit"] = timeUnit;
                tsArray->setAttributes(tsAttrs);
            }
            if (auto res = tsArray->open(); !res) {
                mod.raiseError(QStringLiteral("Failed to open timestamps array: ") + res.error());
                tsArray.reset();
                writeData = false;
                return;
            }

            dataArray = std::make_unique<ZarrV3Array>(
                QString::fromStdString(storePath),
                QStringLiteral("data"),
                ZarrV3Array::DType::UInt32,
                chunkCount,
                2,
                QStringList{QStringLiteral("line"), QStringLiteral("value")});
            QJsonObject dataAttrs;
            dataAttrs["signal_names"] = QJsonArray{QStringLiteral("line_id"), QStringLiteral("value")};
            if (!dataUnit.isEmpty())
                dataAttrs["data_unit"] = dataUnit;
            if (currentDSet)
                dataAttrs["collection_id"] = QString::fromStdString(currentDSet->collectionId().toHex());
            dataArray->setAttributes(dataAttrs);
            if (auto res = dataArray->open(); !res) {
                mod.raiseError(QStringLiteral("Failed to open data array: ") + res.error());
                tsArray.reset();
                dataArray.reset();
                writeData = false;
                return;
            }
        }

        void onDataReceived()
        {
            // Drain everything queued - even when not saving or after a fatal
            // error - otherwise items pile up unbounded.
            const ProcessVarFn processItem = [this](BaseDataType &data) {
                if (!writeData)
                    return;
                if (lineEvents)
                    writeLineReading(static_cast<const LineReading &>(data));
                else
                    visitSignalBlock(data, [this](auto &block) {
                        writeSignalBlock(block);
                    });
                if (writeData)
                    itemsWritten++;
            };
            while (sub->callIfNextVar(processItem)) {
            }
        }

        template<SignalBlockType T>
        void writeSignalBlock(const T &block)
        {
            using Scalar = signal_block_scalar_t<T>;

            ensureArraysInitialized(static_cast<int>(block.data.cols()), zarrDTypeFor<Scalar>());
            if (!writeData)
                return; // ensureArraysInitialized hit a fatal condition (channel mismatch, etc.)

            // Syntalos signal-block matrices are row-major.
            // Verify at compile time so we can write the storage directly without a copy.
            static_assert(
                std::remove_reference_t<decltype(block.data)>::IsRowMajor,
                "SignalBlock data matrix must be row-major");

            tsArray->appendBytes(block.timestamps.data(), block.timestamps.rows());
            dataArray->appendBytes(block.data.data(), block.data.rows());

            // Surface any sticky I/O error from the writer back to the user.
            if (tsArray->hasError() || dataArray->hasError()) {
                const QString msg = tsArray->hasError() ? tsArray->errorMessage() : dataArray->errorMessage();
                mod.raiseError(QStringLiteral("Zarr writer I/O error: ") + msg);
                writeData = false;
            }
        }

        void writeLineReading(const LineReading &ev)
        {
            // Append the timestamp and the [line_id, value] row in lockstep so
            // they stay aligned by index.
            ensureLineArraysInitialized();
            if (!writeData)
                return;

            const uint64_t t = static_cast<uint64_t>(ev.time.count());
            const uint32_t row[2] = {static_cast<uint32_t>(ev.lineId), static_cast<uint32_t>(ev.value)};
            tsArray->appendBytes(&t, 1);
            dataArray->appendBytes(row, 1);

            if (tsArray->hasError() || dataArray->hasError()) {
                const QString msg = tsArray->hasError() ? tsArray->errorMessage() : dataArray->errorMessage();
                mod.raiseError(QStringLiteral("Zarr writer I/O error: ") + msg);
                writeData = false; // the caller keeps draining the queue
            }
        }
    };

    void stop() override
    {
        m_settingsDlg->setRunning(false);

        // our event thread is done at this point, so we can read its results and complete the arrays
        auto worker = takeWorker<Worker>();
        setRunStatistic(QStringLiteral("items_written"), worker ? worker->itemsWritten : qint64(0));

        if (!worker || !worker->writeData)
            return;

        if (worker->tsArray && !worker->tsArray->finalize())
            raiseError(QStringLiteral("Failed to finalize Zarr timestamps array"));
        if (worker->dataArray && !worker->dataArray->finalize())
            raiseError(QStringLiteral("Failed to finalize Zarr data array"));
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert(QStringLiteral("input_type"), m_settingsDlg->selectedTypeName());
        settings.insert(QStringLiteral("use_name_from_source"), m_settingsDlg->useNameFromSource());
        settings.insert(QStringLiteral("data_name"), m_settingsDlg->dataName());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDlg->setSelectedTypeName(settings.value(QStringLiteral("input_type")).toString());
        updatePortConfiguration();
        m_settingsDlg->setUseNameFromSource(settings.value(QStringLiteral("use_name_from_source"), true).toBool());
        m_settingsDlg->setDataName(settings.value(QStringLiteral("data_name")).toString());
        return true;
    }

private:
    static int64_t chunkCountFromSampleRate(double sampleRate)
    {
        // Target ~1 second per inner chunk. Clamped so very low-rate streams
        // still get a reasonably-sized chunk and very high-rate streams don't
        // create excessively large ones.
        if (sampleRate <= 0)
            return ZARR_CHUNK_DEFAULT;
        const auto count = static_cast<int64_t>(std::round(sampleRate));
        return std::clamp(count, ZARR_CHUNK_MIN, ZARR_CHUNK_MAX);
    }
};

QString ZarrWriterModuleInfo::id() const
{
    return QStringLiteral("zarrwriter");
}

QString ZarrWriterModuleInfo::name() const
{
    return QStringLiteral("Zarr Writer");
}

QString ZarrWriterModuleInfo::description() const
{
    return QStringLiteral("Write incoming signal data as a Zarr array store.");
}

ModuleCategories ZarrWriterModuleInfo::categories() const
{
    return ModuleCategory::WRITERS;
}

QColor ZarrWriterModuleInfo::color() const
{
    return QColor::fromString("#e58077");
}

AbstractModule *ZarrWriterModuleInfo::createModule(QObject *parent)
{
    return new ZarrWriterModule(this, parent);
}

#include "zarrwritermodule.moc"
