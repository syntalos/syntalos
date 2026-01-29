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

#include "plotseriesmodule.h"

#include "plotwindow.h"
#include "timeplotwidget.h"

SYNTALOS_MODULE(PlotSeriesModule)

template<typename T>
class PlotSubscriptionDetails
{
public:
    explicit PlotSubscriptionDetails(std::shared_ptr<StreamInputPort<T>> newPort, TimePlotWidget *plot)
        : port(newPort),
          plotWidget(plot),
          timestampDivisor(1000)
    {
        sub = port->subscription();
    }

    std::shared_ptr<StreamInputPort<T>> port;
    std::shared_ptr<StreamSubscription<T>> sub;
    std::vector<bool> showSignal;
    TimePlotWidget *plotWidget;

    int expectedSigSeriesCount;
    double timestampDivisor;
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
        m_plotWindow->updatePortLists();
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
        for (auto &port : inPorts()) {
            auto plotWidget = m_plotWindow->plotWidgetForPort(port->id());

            // we can't or shouldn't display anything if we have no plot widget
            if (plotWidget == nullptr) {
                if (port->hasSubscription())
                    port->subscriptionVar()->suspend();
                continue;
            }

            // do nothing if we have no subscription
            if (!port->hasSubscription()) {
                plotWidget->setVisible(false);
                continue;
            }
            plotWidget->setVisible(true);
            plotWidget->clear();

            // resume, in case we previously suspended the subscription
            port->subscriptionVar()->resume();

            if (port->dataTypeName() == "FloatSignalBlock") {
                PlotSubscriptionDetails<FloatSignalBlock> sdF(
                    std::static_pointer_cast<StreamInputPort<FloatSignalBlock>>(port), plotWidget);
                m_fpSubs.push_back(sdF);

                // prevent receiving more than 4k items/s to safeguard a bit against overflows
                sdF.sub->setThrottleItemsPerSec(4000);
            } else if (port->dataTypeName() == "IntSignalBlock") {
                PlotSubscriptionDetails<IntSignalBlock> sdI(
                    std::static_pointer_cast<StreamInputPort<IntSignalBlock>>(port), plotWidget);
                m_intSubs.push_back(sdI);

                // prevent receiving more than 4k items/s
                sdI.sub->setThrottleItemsPerSec(4000);
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
        const auto timeUnitStr = sd.sub->metadataValue("time_unit", "milliseconds").toString();
        if (timeUnitStr == "seconds")
            sd.timestampDivisor = 1;
        else if (timeUnitStr == "milliseconds")
            sd.timestampDivisor = 1000;
        else if (timeUnitStr == "microseconds")
            sd.timestampDivisor = 1000 * 1000;
        else if (timeUnitStr == "index") {
            const auto sampleRate = sd.sub->metadataValue("sample_rate", -1).toDouble();
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

        sd.plotWidget->setYAxisLabel(sd.sub->metadataValue("data_unit", "y").toString());

        sd.expectedSigSeriesCount = 0;
        sd.showSignal.clear();

        const auto signalNames = sd.sub->metadataValue("signal_names", QStringList()).toStringList();
        m_plotWindow->setSignalsForPort(sd.port->id(), signalNames);
        for (const auto &name : signalNames) {
            const auto sps = m_plotWindow->signalPlotSettingsFor(sd.port->id(), name);
            if (sps.isVisible) {
                sd.plotWidget->addSeries(name, sps);
                sd.showSignal.push_back(true);
            } else {
                sd.showSignal.push_back(false);
            }

            sd.expectedSigSeriesCount++;
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
    }

    template<typename T>
    void processIncomingData(PlotSubscriptionDetails<T> &sd)
    {
        auto maybeData = sd.sub->peekNext();
        if (!maybeData.has_value())
            return;
        const auto data = maybeData.value();

        sd.plotWidget->addToTimeseries(data.timestamps, sd.timestampDivisor);

        // sanity check
        if (data.data.cols() != sd.expectedSigSeriesCount) {
            raiseError(QStringLiteral(
                           "Unexpected amount of signal-series received on port %1: Expected %2, but got %3. "
                           "This is a bug in the module we receive data from.")
                           .arg(sd.port->title())
                           .arg(sd.expectedSigSeriesCount)
                           .arg(data.data.cols()));
            return;
        }

        // set the new data
        int seriesIdx = 0;
        for (int i = 0; i < data.data.cols(); ++i) {
            if (!sd.showSignal[i])
                continue;

            if constexpr (std::is_same_v<T, IntSignalBlock>)
                sd.plotWidget->addToSeriesI(seriesIdx, data.data.col(i));
            else
                sd.plotWidget->addToSeriesF(seriesIdx, data.data.col(i));
            seriesIdx++;
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
        QVariantHash varPortSigConfig;
        for (const auto &port : inPorts()) {
            QVariantHash po;
            po.insert("id", port->id());
            po.insert("title", port->title());
            po.insert("data_type", port->dataTypeName());
            varInPorts.append(po);
        }

        for (const auto &port : inPorts()) {
            QVariantList sigSetList;
            for (const auto &sps : m_plotWindow->signalPlotSettingsFor(port->id())) {
                QVariantHash sc;
                sc.insert("name", sps.name);
                sc.insert("is_visible", sps.isVisible);
                sc.insert("is_digital", sps.isDigital);
                sigSetList.append(sc);
            }
            varPortSigConfig[port->id()] = sigSetList;
        }

        settings.insert("ports_in", varInPorts);
        settings.insert("signals_settings", varPortSigConfig);
        settings.insert("settings_panel_visible", m_plotWindow->defaultSettingsVisible());
        settings.insert("update_frequency", m_plotWindow->updateFrequency());
        settings.insert("buffer_size", m_plotWindow->bufferSize());
    }

    bool loadSettings(const QString &, const QVariantHash &settings, const QByteArray &) override
    {
        clearInPorts();

        const auto varInPorts = settings.value("ports_in").toList();
        const auto varPortSigSettings = settings.value("signals_settings").toHash();

        for (const auto &pv : varInPorts) {
            const auto po = pv.toHash();
            const auto portId = po.value("id").toString();
            registerInputPortByTypeId(
                BaseDataType::typeIdFromString(qPrintable(po.value("data_type").toString())),
                portId,
                po.value("title").toString());

            // read settings for the signals associated with this port
            for (const auto &varSigSet : varPortSigSettings.value(portId, QVariantList()).toList()) {
                const auto sigSet = varSigSet.toHash();
                PlotSeriesSettings pss(sigSet.value("name").toString());
                pss.isVisible = sigSet.value("is_visible").toBool();
                pss.isDigital = sigSet.value("is_digital").toBool();

                m_plotWindow->setSignalPlotSettings(portId, pss);
            }
        }

        m_plotWindow->setDefaultSettingsVisible(settings.value("settings_panel_visible").toBool());
        m_plotWindow->setUpdateFrequency(settings.value("update_frequency", 60).toInt());
        m_plotWindow->setBufferSize(settings.value("buffer_size", 1024).toInt());

        m_plotWindow->updatePortLists();
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
