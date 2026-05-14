/*
 * Copyright (C) 2023-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include "plotseriesmodule.h"

#include "plotcanvas.h"
#include "plotwindow.h"

SYNTALOS_MODULE(PlotSeriesModule)

template<typename T>
class PlotSubscriptionDetails
{
public:
    explicit PlotSubscriptionDetails(std::shared_ptr<StreamInputPort<T>> newPort)
        : port(newPort),
          timestampDivisor(1000)
    {
        sub = port->subscription();
        portId = port->id();
    }

    std::shared_ptr<StreamInputPort<T>> port;
    std::shared_ptr<StreamSubscription<T>> sub;
    QString portId;
    double timestampDivisor;
    // Resolved canvas channel indices by column; -1 = not yet resolved.
    std::vector<int> channelIdxByCol;
};

class PlotSeriesModule : public AbstractModule
{
    Q_OBJECT
private:
    std::vector<PlotSubscriptionDetails<FloatSignalBlock>> m_fpSubs;
    std::vector<PlotSubscriptionDetails<IntSignalBlock>> m_intSubs;

    PlotWindow *m_plotWindow;
    bool m_active;

public:
    explicit PlotSeriesModule(PlotSeriesModuleInfo *modInfo, QObject *parent = nullptr)
        : AbstractModule(parent)
    {
        // Register default input ports
        registerInputPort<FloatSignalBlock>(QStringLiteral("fpsig1-in"), QStringLiteral("Float In 1"));
        registerInputPort<IntSignalBlock>(QStringLiteral("intsig1-in"), QStringLiteral("Int In 1"));

        m_plotWindow = new PlotWindow(this);
        m_plotWindow->setWindowIcon(modInfo->icon());
        addDisplayWindow(m_plotWindow);
        m_plotWindow->refreshChannelTable();
    }

    ~PlotSeriesModule() override {}

    ModuleFeatures features() const override
    {
        return ModuleFeature::SHOW_DISPLAY;
    }

    ModuleDriverKind driver() const override
    {
        return ModuleDriverKind::EVENTS_DEDICATED;
    }

    bool prepare(const TestSubject &) override
    {
        m_active = false;
        m_fpSubs.clear();
        m_intSubs.clear();

        auto canvas = m_plotWindow->canvas();
        canvas->clearRuntimeData();

        for (auto &port : inPorts()) {
            if (!port->hasSubscription()) {
                // do nothing if we have no subscription
                if (port->hasSubscription())
                    port->subscriptionVar()->suspend();
                continue;
            }

            // resume, in case we previously suspended the subscription
            port->subscriptionVar()->resume();

            // Register port on the canvas with default divisor; updated in start()
            // once metadata is available.
            canvas->registerPort(port->id(), 1000.0, QStringLiteral("y"));

            if (port->dataTypeName() == "FloatSignalBlock") {
                PlotSubscriptionDetails<FloatSignalBlock> sd(
                    std::static_pointer_cast<StreamInputPort<FloatSignalBlock>>(port));

                // prevent receiving more than 4k items/s to safeguard a bit against overflows
                sd.sub->setThrottleItemsPerSec(4000);
                m_fpSubs.push_back(sd);
            } else if (port->dataTypeName() == "IntSignalBlock") {
                PlotSubscriptionDetails<IntSignalBlock> sd(
                    std::static_pointer_cast<StreamInputPort<IntSignalBlock>>(port));

                // prevent receiving more than 4k items/s
                sd.sub->setThrottleItemsPerSec(4000);
                m_intSubs.push_back(sd);
            } else {
                continue;
            }

            registerDataReceivedEvent(&PlotSeriesModule::onSignalBlockReceived, port->subscriptionVar());
        }

        // we are only active if we have something subscribed
        if (!m_fpSubs.empty() || !m_intSubs.empty())
            m_active = true;

        // success
        setStateReady();
        return true;
    }

    template<typename T>
    void applyMetadataForSubscription(PlotSubscriptionDetails<T> &sd)
    {
        const auto timeUnitStr = sd.sub->metadataValue("time_unit", std::string{"milliseconds"});
        if (timeUnitStr == "seconds")
            sd.timestampDivisor = 1;
        else if (timeUnitStr == "milliseconds")
            sd.timestampDivisor = 1000;
        else if (timeUnitStr == "microseconds")
            sd.timestampDivisor = 1000 * 1000;
        else if (timeUnitStr == "index") {
            const auto sampleRate = sd.sub->metadataValue("sample_rate", -1.0);
            if (sampleRate < 0) {
                raiseError(QStringLiteral(
                               "The signal-series on port %1 provides timestamps at indices, but no "
                               "\"sample_rate\" metadata value.\n"
                               "This value is needed to calculate timestamps. This is a bug in the module "
                               "we receive data from.")
                               .arg(sd.port->title()));
                return;
            }
            sd.timestampDivisor = sampleRate;
        }

        const QString yLabel = QString::fromStdString(sd.sub->metadataValue("data_unit", std::string{"y"}));
        m_plotWindow->canvas()->registerPort(sd.portId, sd.timestampDivisor, yLabel);

        // Pre-create channel entries from any signal_names metadata so the table
        // is populated before data starts flowing.
        const auto sigNamesArr = sd.sub->metadataValue("signal_names", MetaArray{});
        int colIdx = 0;
        for (const auto &v : sigNamesArr) {
            QString name = QStringLiteral("ch%1").arg(colIdx);
            if (const auto s = v.template get<std::string>())
                name = QString::fromStdString(*s);
            m_plotWindow->canvas()->ensureChannel(sd.portId, colIdx, name);
            ++colIdx;
        }
    }

    void start() override
    {
        m_plotWindow->setRunning(true);

        // apply all metadata
        for (auto &sd : m_fpSubs)
            applyMetadataForSubscription(sd);
        for (auto &sd : m_intSubs)
            applyMetadataForSubscription(sd);

        m_plotWindow->refreshChannelTable();
    }

    template<typename T>
    void processIncomingData(PlotSubscriptionDetails<T> &sd)
    {
        auto canvas = m_plotWindow->canvas();

        // Drain all queued blocks in a tight loop to prevent the subscription
        // queue from growing when a 30 kHz producer outruns individual event firings.
        while (true) {
            auto maybeData = sd.sub->peekNext();
            if (!maybeData.has_value())
                break;
            const auto &data = *maybeData;
            const int nCols = data.data.cols();

            // Grow the index cache on first sight of a new column count.
            if ((int)sd.channelIdxByCol.size() < nCols)
                sd.channelIdxByCol.resize(nCols, -1);

            // Resolve any still-unknown channel indices (at most once per column).
            for (int c = 0; c < nCols; ++c) {
                if (sd.channelIdxByCol[c] == -1)
                    sd.channelIdxByCol[c] = canvas->ensureChannel(sd.portId, c, QString());
            }

            // One lock acquisition per block for all channels.
            if constexpr (std::is_same_v<T, IntSignalBlock>)
                canvas->appendBlockI(sd.portId, data.timestamps, data.data, sd.channelIdxByCol.data(), nCols);
            else
                canvas->appendBlockF(sd.portId, data.timestamps, data.data, sd.channelIdxByCol.data(), nCols);
        }
    }

    void onSignalBlockReceived()
    {
        if (!m_active)
            return;

        for (auto &sd : m_fpSubs)
            processIncomingData(sd);
        for (auto &sd : m_intSubs)
            processIncomingData(sd);
    }

    void stop() override
    {
        m_active = false;
        m_plotWindow->setRunning(false);
    }

    void serializeSettings(const QString &, QVariantHash &settings, QByteArray &) override
    {
        QVariantList varInPorts;
        for (const auto &port : inPorts()) {
            QVariantHash po;
            po.insert("id", port->id());
            po.insert("title", port->title());
            po.insert("data_type", port->dataTypeName());
            varInPorts.append(po);
        }

        settings.insert("ports_in", varInPorts);
        settings.insert("channels", m_plotWindow->canvas()->saveChannels());
        settings.insert("graphs", m_plotWindow->canvas()->saveGraphs());
        settings.insert("settings_panel_visible", m_plotWindow->defaultSettingsVisible());
        settings.insert("update_frequency", m_plotWindow->updateFrequency());
        settings.insert("buffer_size", m_plotWindow->bufferSize());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        clearInPorts();
        m_plotWindow->canvas()->clearAll();

        const auto varInPorts = settings.value("ports_in").toList();
        for (const auto &pv : varInPorts) {
            const auto po = pv.toHash();
            const auto portId = po.value("id").toString();
            registerInputPortByTypeId(
                BaseDataType::typeIdFromString(qPrintable(po.value("data_type").toString())),
                portId,
                po.value("title").toString());
        }

        // Pre-create canvas ports so loaded channels can resolve later.
        for (const auto &pv : varInPorts) {
            const auto po = pv.toHash();
            m_plotWindow->canvas()->registerPort(po.value("id").toString(), 1000.0, QStringLiteral("y"));
        }
        m_plotWindow->canvas()->loadChannels(settings.value("channels").toList());
        m_plotWindow->canvas()->loadGraphs(settings.value("graphs").toList());

        m_plotWindow->setDefaultSettingsVisible(settings.value("settings_panel_visible").toBool());
        m_plotWindow->setUpdateFrequency(settings.value("update_frequency", 60).toInt());
        m_plotWindow->setBufferSize(settings.value("buffer_size", 512).toInt());

        m_plotWindow->refreshChannelTable();
        return true;
    }

private:
};

QString PlotSeriesModuleInfo::id() const
{
    return QStringLiteral("plot-timeseries");
}

QString PlotSeriesModuleInfo::name() const
{
    return QStringLiteral("Plot Time Series");
}

QString PlotSeriesModuleInfo::description() const
{
    return QStringLiteral("Plot data as live time series");
}

ModuleCategories PlotSeriesModuleInfo::categories() const
{
    return ModuleCategory::DISPLAY;
}

AbstractModule *PlotSeriesModuleInfo::createModule(QObject *parent)
{
    return new PlotSeriesModule(this, parent);
}

#include "plotseriesmodule.moc"
