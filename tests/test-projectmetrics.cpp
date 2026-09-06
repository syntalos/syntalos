#include <QtTest>

#include "projectmetrics.h"
#include "utils/misc.h"

using namespace Syntalos;

class TestProjectMetrics : public QObject
{
    Q_OBJECT
private slots:

    void runHistoryRoundtrip()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        const auto storeFname = tmpDir.filePath("metrics.json");
        const auto projectFname = tmpDir.filePath("project.syct");

        {
            ProjectMetrics pm(storeFname);
            QVERIFY(!pm.hasRunHistory());

            // unsaved projects have no history and record nothing
            pm.setProjectFile(QString());
            ProjectRunMetrics run;
            run.bytesWritten = 42;
            run.durationSec = 1;
            pm.recordRun(run);
            QVERIFY(!pm.hasRunHistory());

            pm.setProjectFile(projectFname);
            QVERIFY(!pm.hasRunHistory());
            QCOMPARE(pm.maxRunBytes(), 0);
            QCOMPARE(pm.lastRunWriteRateBps(), 0.0);

            for (int i = 1; i <= ProjectMetrics::MaxRunsPerProject + 2; ++i) {
                ProjectRunMetrics r;
                r.finished = QDateTime::currentDateTime();
                r.bytesWritten = i * 1000;
                r.durationSec = 10;
                r.success = (i % 2) == 0;
                pm.recordRun(r);
            }
        }

        // reopen the store and verify the data
        ProjectMetrics pm(storeFname);
        pm.setProjectFile(projectFname);
        QVERIFY(pm.hasRunHistory());
        const auto history = pm.runHistory();
        QCOMPARE(history.size(), ProjectMetrics::MaxRunsPerProject);

        // most recent first, oldest runs dropped
        QCOMPARE(history.first().bytesWritten, 7000);
        QCOMPARE(history.last().bytesWritten, 3000);
        QCOMPARE(history.first().success, false);
        QCOMPARE(pm.maxRunBytes(), 7000);
        QCOMPARE(pm.lastRunWriteRateBps(), 700.0);

        // a different project has no history
        pm.setProjectFile(tmpDir.filePath("other.syct"));
        QVERIFY(!pm.hasRunHistory());
    }

    void projectPruning()
    {
        QTemporaryDir tmpDir;
        QVERIFY(tmpDir.isValid());
        ProjectMetrics pm(tmpDir.filePath("metrics.json"));

        ProjectRunMetrics run;
        run.finished = QDateTime::currentDateTime();
        run.bytesWritten = 100;
        run.durationSec = 1;

        const int projectsN = ProjectMetrics::MaxProjects + 4;
        for (int i = 0; i < projectsN; ++i) {
            pm.setProjectFile(tmpDir.filePath(QStringLiteral("project-%1.syct").arg(i)));
            pm.recordRun(run);
            QTest::qWait(2); // ensure a well-defined last-used order
        }

        // the most recently used projects survive, the oldest are forgotten
        pm.setProjectFile(tmpDir.filePath(QStringLiteral("project-%1.syct").arg(projectsN - 1)));
        QVERIFY(pm.hasRunHistory());

        int known = 0;
        for (int i = 0; i < projectsN; ++i) {
            pm.setProjectFile(tmpDir.filePath(QStringLiteral("project-%1.syct").arg(i)));
            if (pm.hasRunHistory())
                known++;
        }
        QVERIFY(known <= ProjectMetrics::MaxProjects);
        QVERIFY(known >= ProjectMetrics::MaxProjects - 4);
    }
};

QTEST_MAIN(TestProjectMetrics)
#include "test-projectmetrics.moc"
