
#include <QCoreApplication>
#include <syntalos-mlink>

using namespace Syntalos;

class MyCppModule : public SyntalosLinkModule
{
    Q_GADGET
private:
    std::shared_ptr<OutputPortLink<TableRow>> m_tabOut;

public:
    explicit MyCppModule(SyntalosLink *slink)
        : SyntalosLinkModule(slink)
    {
        // Register some example ports
        m_tabOut = registerOutputPort<TableRow>("table-out", "Example Out");
        registerInputPort<TableRow>("table-in", "Example In", this, &MyCppModule::onTableDataReceived);

        // notify that initialization is done and the module is idle now
        setState(ModuleState::IDLE);
    }

    ~MyCppModule() = default;

    bool prepare(const QByteArray &) override
    {
        // Actions to prepare an acquisition run go here!

        // success, we need to signal "ready" here
        setState(ModuleState::READY);
        return true;
    }

    void start() override
    {
        // Actions to perform immediately before data is first acquired go here
    }

    void onTableDataReceived(const TableRow &row)
    {
        // we just fast-forward the row without any edits to the output port
        m_tabOut->submit(row);
    }

    void stop() override
    {
        // Actions to perform once the run is stopped go here
    }
};

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // Initialize link to Syntalos. There can only be one.
    auto slink = initSyntalosModuleLink();

    // Create & run module
    auto mod = std::make_unique<MyCppModule>(slink.get());
    slink->awaitDataForever();

    return a.exec();
}
