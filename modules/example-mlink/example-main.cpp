/*
 * Copyright 2025-2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * SPDX-License-Identifier: MIT
 */

#include <fstream>
#include <iostream>
#include <syntalos-mlink>

using namespace Syntalos;

class ExampleModule : public SyntalosLinkModule
{
private:
    std::shared_ptr<InputPortInfo> m_tabIn;
    std::shared_ptr<OutputPortLink<TableRow>> m_tabOut;

    std::shared_ptr<EDLDataset> m_dataset;
    fs::path m_logFilePath;
    int m_rowCount{0};

public:
    explicit ExampleModule(SyntalosLink *slink)
        : SyntalosLinkModule(slink)
    {
        // Register some example ports
        m_tabOut = registerOutputPortOrAbort<TableRow>("table-out", "Example Out");
        m_tabIn = registerInputPortOrAbort<TableRow>(
            "table-in", "Example In", this, &ExampleModule::onTableDataReceived);

        // notify that initialization is done and the module is idle now
        setState(ModuleState::IDLE);
    }

    ~ExampleModule() override = default;

    bool prepare() override
    {
        // Actions to prepare an acquisition run go here!
        m_tabOut->setMetadataVar("table_header", m_tabIn->metadata().valueOr("table_header", MetaArray{}));

        // Create the default EDL dataset for this module and record metadata
        auto dsetResult = createDefaultDataset();
        if (!dsetResult) {
            raiseError("Failed to create EDL dataset: " + dsetResult.error());
            return false;
        }
        m_dataset = *dsetResult;
        m_dataset->insertAttribute("module_generator", std::string("example-mlink"));
        m_dataset->setDataScanPattern("rows*.tsv", "Received table rows");

        m_logFilePath = m_dataset->setDataFile("rows.tsv", "Example data");
        m_rowCount = 0;

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
        m_rowCount++;

        // Write each received row to the log file
        if (m_dataset) {
            std::ofstream f(m_logFilePath, std::ios::app);
            for (size_t i = 0; i < row.data.size(); ++i) {
                if (i > 0)
                    f << '\t';
                f << row.data[i];
            }
            f << '\n';
        }
    }

    void stop() override
    {
        // Write a short summary attribute at the end of the run
        if (m_dataset)
            m_dataset->insertAttribute("row_count", m_rowCount);

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
