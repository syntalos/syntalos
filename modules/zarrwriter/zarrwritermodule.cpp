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

SYNTALOS_MODULE(ZarrWriterModule)

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
        m_cbNameFromSrc->setEnabled(!running);
        m_nameEdit->setEnabled(!running && !m_cbNameFromSrc->isChecked());
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

private:
    QCheckBox *m_cbNameFromSrc;
    QLineEdit *m_nameEdit;
};

enum class InputSourceKind {
    NONE,
    FLOAT,
    INT,
    UINT16,
    LINE_READING
};

class ZarrWriterModule : public AbstractModule
{
    Q_OBJECT

private:
    // Chunk limits, see chunkCountFromSampleRate() for details
    static constexpr int64_t ZARR_CHUNK_MIN = 1000;
    static constexpr int64_t ZARR_CHUNK_MAX = 100000;
    static constexpr int64_t ZARR_CHUNK_DEFAULT = 10000; // fallback when sample rate is unknown

    std::shared_ptr<StreamInputPort<SignalBlockF32>> m_floatIn;
    std::shared_ptr<StreamInputPort<SignalBlockI32>> m_intIn;
    std::shared_ptr<StreamInputPort<SignalBlockU16>> m_uint16In;
    std::shared_ptr<StreamInputPort<LineReading>> m_lineIn;

    std::shared_ptr<StreamSubscription<SignalBlockF32>> m_floatSub;
    std::shared_ptr<StreamSubscription<SignalBlockI32>> m_intSub;
    std::shared_ptr<StreamSubscription<SignalBlockU16>> m_uint16Sub;
    std::shared_ptr<StreamSubscription<LineReading>> m_lineSub;

    InputSourceKind m_isrcKind;
    bool m_writeData;
    int m_expectedChannels; // 0 = not advertised by upstream, skip channel count validation
    int64_t m_chunkCount;

    std::shared_ptr<EDLDataset> m_currentDSet;
    std::string m_storePath;

    QStringList m_signalNames;
    QString m_timeUnit;
    QString m_dataUnit;
    QString m_srcModType;
    double m_dataScale = 1.0;
    double m_dataOffset = 0.0;
    double m_sampleRate = -1.0;

    std::unique_ptr<ZarrV3Array> m_tsArray;
    std::unique_ptr<ZarrV3Array> m_dataArray;

    ZarrSettingsDialog *m_settingsDlg;

public:
    explicit ZarrWriterModule(ZarrWriterModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent),
          m_isrcKind(InputSourceKind::NONE),
          m_writeData(false),
          m_expectedChannels(0),
          m_chunkCount(ZARR_CHUNK_DEFAULT)
    {
        m_floatIn = registerInputPort<SignalBlockF32>(QStringLiteral("fpsig1-in"), QStringLiteral("Float32 Signals"));
        m_intIn = registerInputPort<SignalBlockI32>(QStringLiteral("intsig1-in"), QStringLiteral("Int32 Signals"));
        m_uint16In = registerInputPort<SignalBlockU16>(
            QStringLiteral("uint16sig1-in"),
            QStringLiteral("UInt16 Signals"));
        m_lineIn = registerInputPort<LineReading>(QStringLiteral("lines1-in"), QStringLiteral("Line Readings"));

        m_settingsDlg = new ZarrSettingsDialog();
        m_settingsDlg->setWindowIcon(modInfo->icon());
        addSettingsWindow(m_settingsDlg);
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

    bool prepare(const TestSubject &) override
    {
        clearDataReceivedEventRegistrations();
        m_isrcKind = InputSourceKind::NONE;
        m_settingsDlg->setRunning(true);

        if (!m_settingsDlg->useNameFromSource() && m_settingsDlg->dataName().isEmpty()) {
            raiseError(QStringLiteral("Dataset name is not set. Please set it in the settings to continue."));
            return false;
        }

        m_writeData = !isEphemeralRun();

        m_floatSub.reset();
        if (m_floatIn->hasSubscription()) {
            m_floatSub = m_floatIn->subscription();
            m_isrcKind = InputSourceKind::FLOAT;
            registerDataReceivedEvent(&ZarrWriterModule::onFloatSignalBlockReceived, m_floatSub);
        }

        m_intSub.reset();
        if (m_intIn->hasSubscription()) {
            m_intSub = m_intIn->subscription();
            if (m_isrcKind != InputSourceKind::NONE) {
                raiseError(QStringLiteral(
                    "More than one input is connected. Only one signal type can be "
                    "written into a Zarr array at a time."));
                return false;
            }
            m_isrcKind = InputSourceKind::INT;
            registerDataReceivedEvent(&ZarrWriterModule::onIntSignalBlockReceived, m_intSub);
        }

        m_uint16Sub.reset();
        if (m_uint16In->hasSubscription()) {
            m_uint16Sub = m_uint16In->subscription();
            if (m_isrcKind != InputSourceKind::NONE) {
                raiseError(QStringLiteral(
                    "More than one input is connected. Only one signal type can be "
                    "written into a Zarr array at a time."));
                return false;
            }
            m_isrcKind = InputSourceKind::UINT16;
            registerDataReceivedEvent(&ZarrWriterModule::onUInt16SignalBlockReceived, m_uint16Sub);
        }

        m_lineSub.reset();
        if (m_lineIn->hasSubscription()) {
            m_lineSub = m_lineIn->subscription();
            if (m_isrcKind != InputSourceKind::NONE) {
                raiseError(QStringLiteral(
                    "More than one input is connected. Only one signal type can be "
                    "written into a Zarr array at a time."));
                return false;
            }
            m_isrcKind = InputSourceKind::LINE_READING;
            registerDataReceivedEvent(&ZarrWriterModule::onLineReadingReceived, m_lineSub);
        }

        setStateReady();
        return true;
    }

    void start() override
    {
        if (m_isrcKind == InputSourceKind::NONE || !m_writeData)
            return;

        // collect stream metadata
        MetaStringMap mdata;
        switch (m_isrcKind) {
        case InputSourceKind::FLOAT: {
            mdata = m_floatSub->metadata();
            m_signalNames.clear();
            for (const auto &v : m_floatSub->metadataValue("signal_names", MetaArray{}))
                if (const auto s = v.get<std::string>())
                    m_signalNames << QString::fromStdString(*s);
            m_timeUnit = QString::fromStdString(m_floatSub->metadataValue("time_unit", std::string{}));
            m_dataUnit = QString::fromStdString(m_floatSub->metadataValue("data_unit", std::string{}));
            m_dataScale = m_floatSub->metadataValue("data_scale", 1.0);
            m_dataOffset = m_floatSub->metadataValue("data_offset", 0.0);
            m_sampleRate = m_floatSub->metadataValue("sample_rate", -1.0);
            m_chunkCount = chunkCountFromSampleRate(m_sampleRate);
            break;
        }
        case InputSourceKind::INT: {
            mdata = m_intSub->metadata();
            m_signalNames.clear();
            for (const auto &v : m_intSub->metadataValue("signal_names", MetaArray{}))
                if (const auto s = v.get<std::string>())
                    m_signalNames << QString::fromStdString(*s);
            m_timeUnit = QString::fromStdString(m_intSub->metadataValue("time_unit", std::string{}));
            m_dataUnit = QString::fromStdString(m_intSub->metadataValue("data_unit", std::string{}));
            m_dataScale = m_intSub->metadataValue("data_scale", 1.0);
            m_dataOffset = m_intSub->metadataValue("data_offset", 0.0);
            m_sampleRate = m_intSub->metadataValue("sample_rate", -1.0);
            m_chunkCount = chunkCountFromSampleRate(m_sampleRate);
            break;
        }
        case InputSourceKind::UINT16: {
            mdata = m_uint16Sub->metadata();
            m_signalNames.clear();
            for (const auto &v : m_uint16Sub->metadataValue("signal_names", MetaArray{}))
                if (const auto s = v.get<std::string>())
                    m_signalNames << QString::fromStdString(*s);
            m_timeUnit = QString::fromStdString(m_uint16Sub->metadataValue("time_unit", std::string{}));
            m_dataUnit = QString::fromStdString(m_uint16Sub->metadataValue("data_unit", std::string{}));
            m_dataScale = m_uint16Sub->metadataValue("data_scale", 1.0);
            m_dataOffset = m_uint16Sub->metadataValue("data_offset", 0.0);
            m_sampleRate = m_uint16Sub->metadataValue("sample_rate", -1.0);
            m_chunkCount = chunkCountFromSampleRate(m_sampleRate);
            break;
        }
        case InputSourceKind::LINE_READING: {
            // Sparse edge events: recorded as a timestamps array + a 2-column
            // [line_id, value] data array. The data columns are fixed, so the
            // upstream signal_names (line labels) are kept only for the dataset
            // attributes, not the array schema.
            mdata = m_lineSub->metadata();
            m_signalNames.clear();
            for (const auto &v : m_lineSub->metadataValue("signal_names", MetaArray{}))
                if (const auto s = v.get<std::string>())
                    m_signalNames << QString::fromStdString(*s);
            m_timeUnit = QString::fromStdString(m_lineSub->metadataValue("time_unit", std::string{"microseconds"}));
            m_dataUnit = QString::fromStdString(m_lineSub->metadataValue("data_unit", std::string{}));
            m_chunkCount = chunkCountFromSampleRate(-1.0); // events are irregular: use default chunk size
            break;
        }
        default:
            return;
        }

        m_srcModType = QString::fromStdString(mdata.valueOr<std::string>("src_mod_type", std::string{}));

        // create EDL dataset for this recording
        if (m_settingsDlg->useNameFromSource())
            m_currentDSet = createDefaultDataset(name(), mdata);
        else
            m_currentDSet = createDefaultDataset(m_settingsDlg->dataName());

        if (!m_currentDSet) {
            m_writeData = false;
            return;
        }

        // Mirror the signal metadata into the dataset's attributes.toml so the EDL
        // manifest is self-describing without parsing the Zarr store's zarr.json.
        if (m_sampleRate > 0 || m_timeUnit == "index")
            m_currentDSet->insertAttribute("sample_rate", m_sampleRate);
        if (!m_timeUnit.isEmpty())
            m_currentDSet->insertAttribute("time_unit", m_timeUnit.toStdString());
        if (!m_dataUnit.isEmpty())
            m_currentDSet->insertAttribute("data_unit", m_dataUnit.toStdString());
        if (m_dataScale != 1.0)
            m_currentDSet->insertAttribute("data_scale", m_dataScale);
        if (m_dataOffset != 0.0)
            m_currentDSet->insertAttribute("data_offset", m_dataOffset);
        if (!m_srcModType.isEmpty())
            m_currentDSet->insertAttribute("src_mod_type", m_srcModType.toStdString());
        if (!m_signalNames.isEmpty()) {
            MetaArray names;
            for (const auto &n : m_signalNames)
                names.push_back(n.toStdString());
            m_currentDSet->insertAttribute("signal_names", names);
        }

        // register the Zarr store directory as the dataset's primary file
        auto storeName = dataBasenameFromSubMetadata(mdata, m_currentDSet->name());
        storeName += ".zarr";
        m_storePath = m_currentDSet->setDataFile(storeName);

        // Write Zarr v3 root group metadata
        if (!zarrWriteRootGroupMetadata(m_storePath)) {
            raiseError(QStringLiteral("Failed to create Zarr store directory or root metadata"));
            m_writeData = false;
            return;
        }

        // arrays are created lazily on the first data block. We store the
        // expected channel count from metadata now so we can validate the
        // actual incoming data against it.
        m_expectedChannels = static_cast<int>(m_signalNames.size()); // 0 = not advertised, skip validation
        m_tsArray.reset();
        m_dataArray.reset();
    }

    void stop() override
    {
        m_settingsDlg->setRunning(false);

        if (!m_writeData)
            return;

        if (m_tsArray && !m_tsArray->finalize())
            raiseError(QStringLiteral("Failed to finalize Zarr timestamps array"));
        if (m_dataArray && !m_dataArray->finalize())
            raiseError(QStringLiteral("Failed to finalize Zarr data array"));

        m_tsArray.reset();
        m_dataArray.reset();
        m_currentDSet.reset();
        m_expectedChannels = 0;
        m_chunkCount = ZARR_CHUNK_DEFAULT;
        m_signalNames.clear();
        m_timeUnit.clear();
        m_dataUnit.clear();
        m_srcModType.clear();
        m_dataScale = 1.0;
        m_dataOffset = 0.0;
        m_sampleRate = -1.0;
        m_storePath.clear();
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert(QStringLiteral("use_name_from_source"), m_settingsDlg->useNameFromSource());
        settings.insert(QStringLiteral("data_name"), m_settingsDlg->dataName());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
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

    void ensureArraysInitialized(int nCols)
    {
        // validate channel count against what the upstream source advertised.
        if (nCols != m_expectedChannels) {
            raiseError(QStringLiteral("Channel count mismatch: metadata advertised %1 channel(s) but received %2")
                           .arg(m_expectedChannels)
                           .arg(nCols));
            m_writeData = false;
            return;
        }

        // skip if we are already initialized
        if (m_tsArray)
            return;

        ZarrV3Array::DType dataDtype;
        switch (m_isrcKind) {
        case InputSourceKind::FLOAT:
            dataDtype = ZarrV3Array::DType::Float32;
            break;
        case InputSourceKind::INT:
            dataDtype = ZarrV3Array::DType::Int32;
            break;
        case InputSourceKind::UINT16:
            dataDtype = ZarrV3Array::DType::UInt16;
            break;
        default:
            raiseError(QStringLiteral("Internal error: unknown input source kind"));
            m_writeData = false;
            return;
        }

        // 1-D timestamps array: shape = [total_samples], dtype = uint64
        m_tsArray = std::make_unique<ZarrV3Array>(
            QString::fromStdString(m_storePath),
            QStringLiteral("timestamps"),
            ZarrV3Array::DType::UInt64,
            m_chunkCount,
            1,
            QStringList{QStringLiteral("time")});

        if (!m_timeUnit.isEmpty()) {
            QJsonObject tsAttrs;
            tsAttrs["time_unit"] = m_timeUnit;
            m_tsArray->setAttributes(tsAttrs);
        }

        if (auto res = m_tsArray->open(); !res) {
            raiseError(QStringLiteral("Failed to open timestamps array: ") + res.error());
            m_tsArray.reset();
            m_writeData = false;
            return;
        }

        // 2-D data array: shape = [total_samples, n_channels]
        const QStringList dataDimNames = {QStringLiteral("time"), QStringLiteral("channel")};
        m_dataArray = std::make_unique<ZarrV3Array>(
            QString::fromStdString(m_storePath),
            QStringLiteral("data"),
            dataDtype,
            m_chunkCount,
            nCols,
            dataDimNames);

        // embed signal metadata as Zarr array attributes. time_unit lives on
        // the timestamps array (which it describes), not here.
        QJsonObject dataAttrs;
        if (!m_signalNames.isEmpty()) {
            QJsonArray names;
            for (const auto &n : m_signalNames)
                names.append(n);
            dataAttrs["signal_names"] = names;
        }
        if (!m_dataUnit.isEmpty())
            dataAttrs["data_unit"] = m_dataUnit;
        if (m_dataScale != 1.0)
            dataAttrs["data_scale"] = m_dataScale;
        if (m_dataOffset != 0.0)
            dataAttrs["data_offset"] = m_dataOffset;
        if (m_sampleRate > 0 || m_dataUnit == "index")
            dataAttrs["sample_rate"] = m_sampleRate;
        if (m_currentDSet)
            dataAttrs["collection_id"] = QString::fromStdString(m_currentDSet->collectionId().toHex());
        if (!dataAttrs.isEmpty())
            m_dataArray->setAttributes(dataAttrs);

        if (auto res = m_dataArray->open(); !res) {
            raiseError(QStringLiteral("Failed to open data array: ") + res.error());
            m_tsArray.reset();
            m_dataArray.reset();
            m_writeData = false;
            return;
        }
    }

    /**
     * Lazily create the arrays for a LineReading event stream: a 1-D
     * `timestamps` array plus a 2-column `data` array holding [line_id, value]
     * per event. Row i of `data` pairs with timestamps[i]; both are appended one
     * row per event in lockstep so the triplet stays aligned. UInt64 is used for
     * both (the DType enum has no UInt32, and UInt64 holds line_id and the full
     * uint32 value losslessly).
     */
    void ensureLineArraysInitialized()
    {
        if (m_tsArray)
            return;

        m_tsArray = std::make_unique<ZarrV3Array>(
            QString::fromStdString(m_storePath),
            QStringLiteral("timestamps"),
            ZarrV3Array::DType::UInt64,
            m_chunkCount,
            1,
            QStringList{QStringLiteral("event")});
        if (!m_timeUnit.isEmpty()) {
            QJsonObject tsAttrs;
            tsAttrs["time_unit"] = m_timeUnit;
            m_tsArray->setAttributes(tsAttrs);
        }
        if (auto res = m_tsArray->open(); !res) {
            raiseError(QStringLiteral("Failed to open timestamps array: ") + res.error());
            m_tsArray.reset();
            m_writeData = false;
            return;
        }

        m_dataArray = std::make_unique<ZarrV3Array>(
            QString::fromStdString(m_storePath),
            QStringLiteral("data"),
            ZarrV3Array::DType::UInt64,
            m_chunkCount,
            2,
            QStringList{QStringLiteral("event"), QStringLiteral("field")});
        QJsonObject dataAttrs;
        dataAttrs["signal_names"] = QJsonArray{QStringLiteral("line_id"), QStringLiteral("value")};
        if (!m_dataUnit.isEmpty())
            dataAttrs["data_unit"] = m_dataUnit;
        if (m_currentDSet)
            dataAttrs["collection_id"] = QString::fromStdString(m_currentDSet->collectionId().toHex());
        m_dataArray->setAttributes(dataAttrs);
        if (auto res = m_dataArray->open(); !res) {
            raiseError(QStringLiteral("Failed to open data array: ") + res.error());
            m_tsArray.reset();
            m_dataArray.reset();
            m_writeData = false;
            return;
        }
    }

    template<typename SubPtr>
    void handleSignalBlock(SubPtr &sub)
    {
        const auto maybeData = sub->peekNext();
        if (!maybeData.has_value() || !m_writeData)
            return;
        const auto &block = maybeData.value();

        ensureArraysInitialized(static_cast<int>(block.data.cols()));
        if (!m_writeData)
            return; // ensureArraysInitialized hit a fatal condition (channel mismatch, etc.)

        // Syntalos signal-block matrices are row-major.
        // Verify at compile time so we can write the storage directly without a copy.
        static_assert(
            std::remove_reference_t<decltype(block.data)>::IsRowMajor,
            "SignalBlock data matrix must be row-major");

        m_tsArray->appendBytes(block.timestamps.data(), block.timestamps.rows());
        m_dataArray->appendBytes(block.data.data(), block.data.rows());

        // Surface any sticky I/O error from the writer back to the user.
        if (m_tsArray->hasError() || m_dataArray->hasError()) {
            const QString msg = m_tsArray->hasError() ? m_tsArray->errorMessage() : m_dataArray->errorMessage();
            raiseError(QStringLiteral("Zarr writer I/O error: ") + msg);
            m_writeData = false;
        }
    }

    void onFloatSignalBlockReceived()
    {
        handleSignalBlock(m_floatSub);
    }

    void onIntSignalBlockReceived()
    {
        handleSignalBlock(m_intSub);
    }

    void onUInt16SignalBlockReceived()
    {
        handleSignalBlock(m_uint16Sub);
    }

    void onLineReadingReceived()
    {
        if (!m_writeData)
            return;

        // Drain all queued events; append the timestamp and the [line_id, value]
        // row in lockstep so they stay aligned by index.
        while (auto maybeData = m_lineSub->peekNext()) {
            const auto &ev = maybeData.value();
            ensureLineArraysInitialized();
            if (!m_writeData)
                return;

            const uint64_t t = static_cast<uint64_t>(ev.time.count());
            const uint64_t row[2] = {static_cast<uint64_t>(ev.lineId), static_cast<uint64_t>(ev.value)};
            m_tsArray->appendBytes(&t, 1);
            m_dataArray->appendBytes(row, 1);

            if (m_tsArray->hasError() || m_dataArray->hasError()) {
                const QString msg = m_tsArray->hasError() ? m_tsArray->errorMessage()
                                                          : m_dataArray->errorMessage();
                raiseError(QStringLiteral("Zarr writer I/O error: ") + msg);
                m_writeData = false;
                return;
            }
        }
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
