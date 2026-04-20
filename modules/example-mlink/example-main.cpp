/*
 * Copyright 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: MIT
 */

#include <syntalos-mlink>

using namespace Syntalos;

class ExampleModule : public SyntalosLinkModule
{
private:
    std::shared_ptr<InputPortInfo> m_tabIn;
    std::shared_ptr<OutputPortLink<TableRow>> m_tabOut;

public:
    explicit ExampleModule(SyntalosLink *slink)
        : SyntalosLinkModule(slink)
    {
        // Register some example ports
        m_tabOut = registerOutputPort<TableRow>("table-out", "Example Out");
        m_tabIn = registerInputPort<TableRow>("table-in", "Example In", this, &ExampleModule::onTableDataReceived);

        // notify that initialization is done and the module is idle now
        setState(ModuleState::IDLE);
    }

    ~ExampleModule() override = default;

    bool prepare() override
    {
        // Actions to prepare an acquisition run go here!
        m_tabOut->setMetadataVar("table_header", m_tabIn->metadata().valueOr("table_header", MetaArray{}));

        // success, we need to signal "ready" here
        setState(ModuleState::READY);
        return true;
    }

    void start() override
    {
        // Actions to perform immediately before data is first acquired go here
        SyntalosLinkModule::start();
    }

    void onTableDataReceived(const TableRow &row)
    {
        // We just fast-forward the row without any edits to the output port
        m_tabOut->submit(row);
    }

    void stop() override
    {
        // Actions to perform once the run is stopped go here
        SyntalosLinkModule::stop();
    }
};

int main(int argc, char *argv[])
{
    // Initialize link to Syntalos. There can only be one.
    auto slink = initSyntalosModuleLink();

    // Create & run module
    auto mod = std::make_unique<ExampleModule>(slink.get());
    slink->awaitDataForever([]() {
        // You can process events from an external event loop here!
    });

    return 0;
}
