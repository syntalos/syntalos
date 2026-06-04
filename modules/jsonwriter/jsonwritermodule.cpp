/*
 * Copyright (C) 2023-2024 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "jsonwritermodule.h"

#include <QUuid>
#include <KCompressionDevice>

#include "jsonsettingsdialog.h"

SYNTALOS_MODULE(JSONWriterModule)

enum class InputSourceKind {
    NONE,
    FLOAT,
    INT,
    ROW,
    LINE_READING
};

static InputSourceKind inputSourceKindFromTypeId(int typeId)
{
    if (typeId == SignalBlockF32::staticTypeId())
        return InputSourceKind::FLOAT;
    if (typeId == SignalBlockI32::staticTypeId())
        return InputSourceKind::INT;
    if (typeId == TableRow::staticTypeId())
        return InputSourceKind::ROW;
    if (typeId == LineReading::staticTypeId())
        return InputSourceKind::LINE_READING;
    return InputSourceKind::NONE;
}

class JSONWriterModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<SignalBlockF32>> m_floatIn;
    std::shared_ptr<StreamInputPort<SignalBlockI32>> m_intIn;
    std::shared_ptr<StreamInputPort<TableRow>> m_rowsIn;
    std::shared_ptr<StreamInputPort<LineReading>> m_lineIn;

    std::shared_ptr<StreamSubscription<SignalBlockF32>> m_floatSub;
    std::shared_ptr<StreamSubscription<SignalBlockI32>> m_intSub;
    std::shared_ptr<StreamSubscription<TableRow>> m_rowSub;
    std::shared_ptr<StreamSubscription<LineReading>> m_lineSub;

    InputSourceKind m_isrcKind;
    std::shared_ptr<EDLDataset> m_currentDSet;

    std::unique_ptr<KCompressionDevice> m_compDev;
    std::unique_ptr<QTextStream> m_textStream;
    bool m_initFile;
    QSet<int> m_selectedIndices;
    bool m_writeData;

    JSONSettingsDialog *m_settingsDlg;

public:
    explicit JSONWriterModule(QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        m_settingsDlg = new JSONSettingsDialog();
        addSettingsWindow(m_settingsDlg);

        connect(m_settingsDlg, &JSONSettingsDialog::settingsChanged, this, [this]() {
            updatePortConfiguration();
        });

        // start with no input ports; the user picks exactly one type in the settings
        updatePortConfiguration();
    }

    void updatePortConfiguration()
    {
        // Rebuild the single input port to match the selected data type.
        // Only safe on the main thread while not running.
        clearInPorts();
        m_floatIn.reset();
        m_intIn.reset();
        m_rowsIn.reset();
        m_lineIn.reset();

        setStatusMessage({});
        switch (inputSourceKindFromTypeId(m_settingsDlg->selectedTypeId())) {
        case InputSourceKind::FLOAT:
            m_floatIn = registerInputPort<SignalBlockF32>(QStringLiteral("f32sig-in"), QStringLiteral("Float Signals"));
            break;
        case InputSourceKind::INT:
            m_intIn = registerInputPort<SignalBlockI32>(QStringLiteral("i32sig-in"), QStringLiteral("Integer Signals"));
            break;
        case InputSourceKind::ROW:
            m_rowsIn = registerInputPort<TableRow>(QStringLiteral("rows-in"), QStringLiteral("Table Rows"));
            break;
        case InputSourceKind::LINE_READING:
            m_lineIn = registerInputPort<LineReading>(QStringLiteral("lines-in"), QStringLiteral("Line Readings"));
            break;
        case InputSourceKind::NONE:
            setStatusMessage("No input port type selected!");
            break;
        }
    }

    ~JSONWriterModule() override {}

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
        m_isrcKind = InputSourceKind::NONE;
        clearDataReceivedEventRegistrations();
        m_settingsDlg->setRunning(true);

        if (!m_settingsDlg->useNameFromSource() && m_settingsDlg->dataName().isEmpty()) {
            raiseError("Data name is not set. Please set it in the settings to continue.");
            return false;
        }

        // we don't write anything to disk if we aren't going to use the data anyway
        m_writeData = !isEphemeralRun();

        // Only a single input port exists at a time
        m_floatSub.reset();
        if (m_floatIn && m_floatIn->hasSubscription()) {
            m_floatSub = m_floatIn->subscription();
            m_isrcKind = InputSourceKind::FLOAT;

            registerDataReceivedEvent(&JSONWriterModule::onFloatSignalBlockReceived, m_floatSub);
        }

        m_intSub.reset();
        if (m_intIn && m_intIn->hasSubscription()) {
            m_intSub = m_intIn->subscription();
            m_isrcKind = InputSourceKind::INT;

            registerDataReceivedEvent(&JSONWriterModule::onIntSignalBlockReceived, m_intSub);
        }

        m_rowSub.reset();
        if (m_rowsIn && m_rowsIn->hasSubscription()) {
            m_rowSub = m_rowsIn->subscription();
            m_isrcKind = InputSourceKind::ROW;

            registerDataReceivedEvent(&JSONWriterModule::onTableRowReceived, m_rowSub);
        }

        m_lineSub.reset();
        if (m_lineIn && m_lineIn->hasSubscription()) {
            m_lineSub = m_lineIn->subscription();
            m_isrcKind = InputSourceKind::LINE_READING;

            registerDataReceivedEvent(&JSONWriterModule::onLineReadingReceived, m_lineSub);
        }

        if (m_isrcKind == InputSourceKind::NONE) {
            // nothing is connected, so there is nothing for us to do this run
            setStateDormant();
            return true;
        }

        // success
        setStateReady();
        return true;
    }

    void start() override
    {
        if (m_isrcKind == InputSourceKind::NONE)
            return;

        MetaStringMap mdata;
        QStringList signalNames;
        switch (m_isrcKind) {
        case InputSourceKind::FLOAT:
            mdata = m_floatSub->metadata();
            for (const auto &v : m_floatSub->metadataValue("signal_names", MetaArray{}))
                if (const auto s = v.get<std::string>())
                    signalNames << QString::fromStdString(*s);
            break;
        case InputSourceKind::INT:
            mdata = m_intSub->metadata();
            for (const auto &v : m_intSub->metadataValue("signal_names", MetaArray{}))
                if (const auto s = v.get<std::string>())
                    signalNames << QString::fromStdString(*s);
            break;
        case InputSourceKind::ROW:
            mdata = m_rowSub->metadata();
            break;
        case InputSourceKind::LINE_READING:
            // Columns are fixed ([time, line_id, value]); no per-signal selection.
            mdata = m_lineSub->metadata();
            break;
        case InputSourceKind::NONE:
            return;
        default:
            return;
        }

        // update GUI to list available signals
        m_settingsDlg->setAvailableEntries(signalNames);

        // convert user signal selection into indices
        m_selectedIndices.clear();
        if (!m_settingsDlg->recordAllData()) {
            const auto recSet = m_settingsDlg->recordedEntriesSet();
            for (int i = 0; i < signalNames.count(); i++) {
                if (recSet.contains(signalNames[i]))
                    m_selectedIndices.insert(i);
            }
        }

        // create dataset for storage
        if (m_settingsDlg->useNameFromSource())
            m_currentDSet = createDefaultDataset(name(), mdata);
        else
            m_currentDSet = createDefaultDataset(m_settingsDlg->dataName());
        if (m_currentDSet.get() == nullptr) {
            m_writeData = false;
            return;
        }

        // get our file basename
        auto fname = dataBasenameFromSubMetadata(mdata, QStringLiteral("data"));
        fname = QStringLiteral("%1.json.zst").arg(fname);

        // retrieve an absolute path from our file basename that we can open
        fname = QString::fromStdString(m_currentDSet->setDataFile(fname.toStdString()));

        m_compDev = std::make_unique<KCompressionDevice>(fname, KCompressionDevice::Zstd);
        if (!m_compDev->open(QIODevice::WriteOnly)) {
            raiseError(QStringLiteral("Unable to open file '%1' for writing: %2").arg(fname, m_compDev->errorString()));
            return;
        }

        m_textStream = std::make_unique<QTextStream>(m_compDev.get());
        m_initFile = true;
    }

    static QString toJsonValue(const QString &str)
    {
        QString res = str;
        res.replace("\\", "\\\\");
        res.replace("\"", "\\\"");
        res.replace("\n", "\\n");
        res.replace("\r", "\\r");
        res.replace("\t", "\\t");

        return "\"" + res + "\"";
    }

    static QString toJsonValue(const std::string &str)
    {
        QString res = QString::fromStdString(str);
        res.replace("\\", "\\\\");
        res.replace("\"", "\\\"");
        res.replace("\n", "\\n");
        res.replace("\r", "\\r");
        res.replace("\t", "\\t");

        return "\"" + res + "\"";
    }

    QString floatToJsonValue(double value)
    {
        if (std::isnan(value))
            return "NaN";

        // this is an extension to the JSON spec that Pandas parses
        if (std::isinf(value))
            return value > 0 ? "Infinity" : "-Infinity";

        return QString::number(value, 'g', 16);
    }

    template<typename T>
    QString intToJsonValue(T value)
    {
        if (std::isnan(value))
            return "NaN";

        // this is an extension to the JSON spec that Pandas parses
        if (std::isinf(value))
            return value > 0 ? "Infinity" : "-Infinity";

        return QString::number(value);
    }

    QString shortenTimeUnit(const QString &timeUnit)
    {
        if (timeUnit == "seconds")
            return QStringLiteral("sec");
        if (timeUnit == "milliseconds")
            return QStringLiteral("msec");
        if (timeUnit == "microseconds")
            return QStringLiteral("usec");
        if (timeUnit == "index")
            return QStringLiteral("idx");

        return "t";
    }

    void initJsonFile()
    {
        QStringList columns;
        QString timeUnit;
        QString dataUnit;
        double dataScale = 1.0;
        double dataOffset = 0.0;
        double sampleRate = -1.0;
        QTextStream &stream = *m_textStream;

        switch (m_isrcKind) {
        case InputSourceKind::FLOAT:
            for (const auto &v : m_floatSub->metadataValue("signal_names", MetaArray{}))
                if (const auto s = v.get<std::string>())
                    columns << QString::fromStdString(*s);
            timeUnit = QString::fromStdString(m_floatSub->metadataValue("time_unit", std::string{}));
            dataUnit = QString::fromStdString(m_floatSub->metadataValue("data_unit", std::string{}));
            dataScale = m_floatSub->metadataValue("data_scale", 1.0);
            dataOffset = m_floatSub->metadataValue("data_offset", 0.0);
            sampleRate = m_floatSub->metadataValue("sample_rate", -1.0);
            break;
        case InputSourceKind::INT:
            for (const auto &v : m_intSub->metadataValue("signal_names", MetaArray{}))
                if (const auto s = v.get<std::string>())
                    columns << QString::fromStdString(*s);
            timeUnit = QString::fromStdString(m_intSub->metadataValue("time_unit", std::string{}));
            dataUnit = QString::fromStdString(m_intSub->metadataValue("data_unit", std::string{}));
            dataScale = m_intSub->metadataValue("data_scale", 1.0);
            dataOffset = m_intSub->metadataValue("data_offset", 0.0);
            sampleRate = m_intSub->metadataValue("sample_rate", -1.0);
            break;
        case InputSourceKind::ROW:
            for (const auto &v : m_rowSub->metadataValue("table_header", MetaArray{}))
                if (const auto s = v.get<std::string>())
                    columns << QString::fromStdString(*s);
            break;
        case InputSourceKind::LINE_READING:
            timeUnit = QString::fromStdString(m_lineSub->metadataValue("time_unit", std::string{"microseconds"}));
            dataUnit = QString::fromStdString(m_lineSub->metadataValue("data_unit", std::string{}));
            // Fixed event schema; the timestamp column is included explicitly
            // (the FLOAT/INT timestamp-prepend below does not run for this kind).
            columns << QStringLiteral("timestamp_%1").arg(shortenTimeUnit(timeUnit)) << QStringLiteral("line_id")
                    << QStringLiteral("value");
            break;
        default:
            return;
        }

        if (columns.isEmpty()) {
            raiseError(
                "Unable to determine the data columns - the data source may not have set the "
                "required `signal_names` or `table_header` metadata. Please ensure the sending module "
                "emits the correct metadata!");
            return;
        }

        if (m_isrcKind == InputSourceKind::FLOAT || m_isrcKind == InputSourceKind::INT) {
            if (timeUnit.isEmpty())
                columns.prepend("timestamp");
            else
                columns.prepend(QStringLiteral("timestamp_%1").arg(shortenTimeUnit(timeUnit)));
        }

        stream << "{";
        if (m_settingsDlg->jsonFormat() == "extended-pandas") {
            stream << "\"collection_id\": " << toJsonValue(m_currentDSet->collectionId().toHex());
            if (!timeUnit.isEmpty())
                stream << ",\n\"time_unit\": " << toJsonValue(timeUnit);
            if (!dataUnit.isEmpty())
                stream << ",\n\"data_unit\": " << toJsonValue(dataUnit);
            if (sampleRate > 0 || timeUnit == "index")
                stream << ",\n\"sample_rate\": " << floatToJsonValue(sampleRate);
            stream << ",\n";
        }

        stream << "\"columns\": [";
        QString columnsLine;
        for (int i = 0; i < columns.length(); i++) {
            // always write timestamp column, then check the other channels for being whitelisted
            if (i == 0)
                columnsLine.append(toJsonValue(columns[i]) + ",");
            else if (m_selectedIndices.isEmpty() || m_selectedIndices.contains(i - 1))
                columnsLine.append(toJsonValue(columns[i]) + ",");
        }
        if (!columnsLine.isEmpty())
            columnsLine = columnsLine.left(columnsLine.length() - 1);
        stream << columnsLine << "],\n\"data\": [\n";

        // add some metadata
        m_currentDSet->insertAttribute("json_schema", m_settingsDlg->jsonFormat().toStdString());
        if (!timeUnit.isEmpty())
            m_currentDSet->insertAttribute("time_unit", timeUnit.toStdString());
        if (!dataUnit.isEmpty())
            m_currentDSet->insertAttribute("data_unit", dataUnit.toStdString());
        // Persist the affine raw->physical transform alongside data_unit
        // so consumers of the EDL manifest can reconstruct physical
        // values. Skip identity values to avoid noise on streams that
        // never advertised the keys.
        if (dataScale != 1.0)
            m_currentDSet->insertAttribute("data_scale", dataScale);
        if (dataOffset != 0.0)
            m_currentDSet->insertAttribute("data_offset", dataOffset);
        if (sampleRate > 0 || timeUnit == "index")
            m_currentDSet->insertAttribute("sample_rate", sampleRate);

        // try to write the header to disk as soon as we can
        stream.flush();
    }

    void writeEntryStart(const VectorXu64 &timestamps, int i)
    {
        if (i == 0 && m_initFile)
            (*m_textStream) << "[" << intToJsonValue(timestamps(i, 0));
        else
            (*m_textStream) << ",\n[" << intToJsonValue(timestamps(i, 0));
    }

    void onFloatSignalBlockReceived()
    {
        auto maybeData = m_floatSub->peekNext();
        if (!maybeData.has_value())
            return;
        const auto &data = *maybeData;
        if (!m_writeData)
            return;

        if (m_initFile)
            initJsonFile();

        for (int i = 0; i < data.timestamps.rows(); ++i) {
            writeEntryStart(data.timestamps, i);

            if (m_selectedIndices.isEmpty()) {
                for (int k = 0; k < data.data.cols(); ++k)
                    (*m_textStream) << "," << intToJsonValue(data.data(i, k));
            } else {
                for (const auto &k : m_selectedIndices)
                    (*m_textStream) << "," << intToJsonValue(data.data(i, k));
            }
            (*m_textStream) << "]";
        }

        // ensure we don't initialize the file twice
        m_initFile = false;
    }

    void onIntSignalBlockReceived()
    {
        auto maybeData = m_intSub->peekNext();
        if (!maybeData.has_value())
            return;
        const auto &data = maybeData.value();
        if (!m_writeData)
            return;

        if (m_initFile)
            initJsonFile();

        for (int i = 0; i < data.timestamps.rows(); ++i) {
            writeEntryStart(data.timestamps, i);

            if (m_selectedIndices.isEmpty()) {
                for (int k = 0; k < data.data.cols(); ++k)
                    (*m_textStream) << "," << floatToJsonValue(data.data(i, k));
            } else {
                for (const auto &k : m_selectedIndices)
                    (*m_textStream) << "," << floatToJsonValue(data.data(i, k));
            }
            (*m_textStream) << "]";
        }

        // ensure we don't initialize the file twice
        m_initFile = false;
    }

    void onTableRowReceived()
    {
        auto maybeData = m_rowSub->peekNext();
        if (!maybeData.has_value())
            return;
        const auto &row = *maybeData;
        if (!m_writeData)
            return;

        if (m_initFile) {
            initJsonFile();
            (*m_textStream) << "[";
        } else {
            (*m_textStream) << ",\n[";
        }

        // ensure we don't initialize the file twice
        m_initFile = false;

        // write row
        for (int i = 0; i < row.length(); i++) {
            if (i == 0)
                (*m_textStream) << toJsonValue(row.data[i]);
            else
                (*m_textStream) << "," << toJsonValue(row.data[i]);
        }
        (*m_textStream) << "]";
    }

    void onLineReadingReceived()
    {
        if (!m_writeData)
            return;

        // One [time, line_id, value] row per edge event.
        while (auto maybeData = m_lineSub->peekNext()) {
            const auto &ev = maybeData.value();

            if (m_initFile) {
                initJsonFile();
                (*m_textStream) << "[";
            } else {
                (*m_textStream) << ",\n[";
            }
            m_initFile = false;

            (*m_textStream) << intToJsonValue(static_cast<uint64_t>(ev.time.count())) << ","
                            << intToJsonValue(ev.lineId) << "," << intToJsonValue(ev.value) << "]";
        }
    }

    void stop() override
    {
        if (m_isrcKind == InputSourceKind::NONE)
            return;

        // write terminator
        if (m_textStream.get() != nullptr) {
            if (m_writeData)
                (*m_textStream) << "\n]}\n";
            m_textStream->flush();
        }

        // close file, reset pointers
        if (m_compDev.get() != nullptr)
            m_compDev->close();
        m_textStream.reset();
        m_compDev.reset();

        m_currentDSet.reset();

        // re-enable UI
        m_settingsDlg->setRunning(false);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        settings.insert("input_type", m_settingsDlg->selectedTypeName());
        settings.insert("use_name_from_source", m_settingsDlg->useNameFromSource());
        settings.insert("data_name", m_settingsDlg->dataName());
        settings.insert("format", m_settingsDlg->jsonFormat());

        settings.insert("record_all", m_settingsDlg->recordAllData());
        settings.insert("available_entries", m_settingsDlg->availableEntries());
        settings.insert("recorded_entries", m_settingsDlg->recordedEntries());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        m_settingsDlg->setSelectedTypeName(settings.value("input_type").toString());
        updatePortConfiguration();

        m_settingsDlg->setUseNameFromSource(settings.value("use_name_from_source", true).toBool());
        m_settingsDlg->setDataName(settings.value("data_name").toString());
        m_settingsDlg->setJsonFormat(settings.value("format").toString());

        m_settingsDlg->setRecordAllData(settings.value("record_all", true).toBool());
        m_settingsDlg->setAvailableEntries(settings.value("available_entries").toStringList());
        m_settingsDlg->setRecordedEntries(settings.value("recorded_entries").toStringList());

        return true;
    }

private:
};

QString JSONWriterModuleInfo::id() const
{
    return QStringLiteral("jsonwriter");
}

QString JSONWriterModuleInfo::name() const
{
    return QStringLiteral("JSON Writer");
}

QString JSONWriterModuleInfo::description() const
{
    return QStringLiteral("Write incoming data into a structured, Pandas-compatible JSON file");
}

ModuleCategories JSONWriterModuleInfo::categories() const
{
    return ModuleCategory::WRITERS;
}

AbstractModule *JSONWriterModuleInfo::createModule(QObject *parent)
{
    return new JSONWriterModule(parent);
}

#include "jsonwritermodule.moc"
