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
    ROW
};

class JSONWriterModule : public AbstractModule
{
    Q_OBJECT
private:
    std::shared_ptr<StreamInputPort<FloatSignalBlock>> m_floatIn;
    std::shared_ptr<StreamInputPort<IntSignalBlock>> m_intIn;
    std::shared_ptr<StreamInputPort<TableRow>> m_rowsIn;

    std::shared_ptr<StreamSubscription<FloatSignalBlock>> m_floatSub;
    std::shared_ptr<StreamSubscription<IntSignalBlock>> m_intSub;
    std::shared_ptr<StreamSubscription<TableRow>> m_rowSub;

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
        // Input ports for all the data we could potentially handle
        m_floatIn = registerInputPort<FloatSignalBlock>(QStringLiteral("fpsig1-in"), QStringLiteral("Float Signals"));
        m_intIn = registerInputPort<IntSignalBlock>(QStringLiteral("intsig1-in"), QStringLiteral("Integer Signals"));
        m_rowsIn = registerInputPort<TableRow>(QStringLiteral("rows"), QStringLiteral("Table Rows"));

        m_settingsDlg = new JSONSettingsDialog();
        addSettingsWindow(m_settingsDlg);
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

        m_floatSub.reset();
        if (m_floatIn->hasSubscription()) {
            m_floatSub = m_floatIn->subscription();
            m_isrcKind = InputSourceKind::FLOAT;

            registerDataReceivedEvent(&JSONWriterModule::onFloatSignalBlockReceived, m_floatSub);
        }

        bool excessConnections = false;
        m_intSub.reset();
        if (m_intIn->hasSubscription()) {
            m_intSub = m_intIn->subscription();
            if (m_isrcKind != InputSourceKind::NONE)
                excessConnections = true;
            m_isrcKind = InputSourceKind::INT;

            registerDataReceivedEvent(&JSONWriterModule::onIntSignalBlockReceived, m_intSub);
        }

        m_rowSub.reset();
        if (m_rowsIn->hasSubscription()) {
            m_rowSub = m_rowsIn->subscription();
            if (m_isrcKind != InputSourceKind::NONE)
                excessConnections = true;
            m_isrcKind = InputSourceKind::ROW;

            registerDataReceivedEvent(&JSONWriterModule::onTableRowReceived, m_rowSub);
        }

        if (excessConnections) {
            raiseError(
                "More than one input port is connected. We can only write data from one modality into a JSON file, "
                "multiplexing is not possible.");
            return false;
        }

        // success
        setStateReady();
        return true;
    }

    void start() override
    {
        if (m_isrcKind == InputSourceKind::NONE)
            return;

        QHash<QString, QVariant> mdata;
        QStringList signalNames;
        switch (m_isrcKind) {
        case InputSourceKind::FLOAT:
            mdata = m_floatSub->metadata();
            signalNames = m_floatSub->metadataValue("signal_names", QStringList()).toStringList();
            break;
        case InputSourceKind::INT:
            mdata = m_intSub->metadata();
            signalNames = m_intSub->metadataValue("signal_names", QStringList()).toStringList();
            break;
        case InputSourceKind::ROW:
            mdata = m_rowSub->metadata();
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

        qDebug() << "Selected:" << m_selectedIndices;

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
        fname = m_currentDSet->setDataFile(fname);

        m_compDev = std::make_unique<KCompressionDevice>(fname, KCompressionDevice::Zstd);
        if (!m_compDev->open(QIODevice::WriteOnly)) {
            raiseError(QStringLiteral("Unable to open file '%1' for writing: %2").arg(fname, m_compDev->errorString()));
            return;
        }

        m_textStream = std::make_unique<QTextStream>(m_compDev.get());
        m_initFile = true;
    }

    static QString toJsonValue(QString str)
    {
        str.replace("\\", "\\\\");
        str.replace("\"", "\\\"");
        str.replace("\n", "\\n");
        str.replace("\r", "\\r");
        str.replace("\t", "\\t");

        return "\"" + str + "\"";
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
        QTextStream &stream = *m_textStream;

        switch (m_isrcKind) {
        case InputSourceKind::FLOAT:
            columns = m_floatSub->metadataValue("signal_names", QStringList()).toStringList();
            timeUnit = m_floatSub->metadataValue("time_unit", QString()).toString();
            dataUnit = m_floatSub->metadataValue("data_unit", QString()).toString();
            break;
        case InputSourceKind::INT:
            columns = m_intSub->metadataValue("signal_names", QStringList()).toStringList();
            timeUnit = m_intSub->metadataValue("time_unit", QString()).toString();
            dataUnit = m_intSub->metadataValue("data_unit", QString()).toString();
            break;
        case InputSourceKind::ROW:
            columns = m_rowSub->metadataValue("table_header", QStringList()).toStringList();
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
            stream << "\"collection_id\": "
                   << toJsonValue(m_currentDSet->collectionId().toString(QUuid::WithoutBraces));
            if (!timeUnit.isEmpty())
                stream << ",\n\"time_unit\": " << toJsonValue(timeUnit);
            if (!dataUnit.isEmpty())
                stream << ",\n\"data_unit\": " << toJsonValue(dataUnit);
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
        m_currentDSet->insertAttribute("json_schema", m_settingsDlg->jsonFormat());
        if (!timeUnit.isEmpty())
            m_currentDSet->insertAttribute("json_time_unit", timeUnit);
        if (!dataUnit.isEmpty())
            m_currentDSet->insertAttribute("json_data_unit", dataUnit);

        // try to write the header to disk as soon as we can
        stream.flush();
    }

    void writeEntryStart(const VectorXu &timestamps, int i)
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
        const auto data = maybeData.value();
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
        const auto data = maybeData.value();
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
        const auto row = maybeData.value();
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
                (*m_textStream) << toJsonValue(row[i]);
            else
                (*m_textStream) << "," << toJsonValue(row[i]);
        }
        (*m_textStream) << "]";
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
        settings.insert("use_name_from_source", m_settingsDlg->useNameFromSource());
        settings.insert("data_name", m_settingsDlg->dataName());
        settings.insert("format", m_settingsDlg->jsonFormat());

        settings.insert("record_all", m_settingsDlg->recordAllData());
        settings.insert("available_entries", m_settingsDlg->availableEntries());
        settings.insert("recorded_entries", m_settingsDlg->recordedEntries());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
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

AbstractModule *JSONWriterModuleInfo::createModule(QObject *parent)
{
    return new JSONWriterModule(parent);
}

#include "jsonwritermodule.moc"
