/*
 * Copyright (C) 2024-2026 Matthias Klumpp <matthias@tenstral.net>
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

#include <QtTest>
#include <atomic>
#include <thread>
#include <syntalos-mlink>

using namespace Syntalos;

class TestMLink : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        // Redirect all directory lookups into a temporary location. This must happen
        // before the first call into the library, as GLib caches these locations.
        QVERIFY(m_tmpDir.isValid());
        qputenv("HOME", m_tmpDir.path().toUtf8());
        qputenv("XDG_DATA_HOME", m_tmpDir.filePath("xdg-data").toUtf8());
        qunsetenv("container");
        qunsetenv("FLATPAK_ID");
    }

    void cleanup()
    {
        qunsetenv("SYNTALOS_MODULE_TYPE_ID");
        qunsetenv("SYNTALOS_MODULE_DIR");
    }

    void moduleCacheDirResolve()
    {
        qputenv("SYNTALOS_MODULE_TYPE_ID", "my-module");
        const auto res = moduleCacheDir(false);
        QVERIFY2(res.has_value(), res.error_or("").c_str());
        QCOMPARE(
            QString::fromStdString(res->string()),
            m_tmpDir.filePath(QStringLiteral("xdg-data/Syntalos/cache/modules/my-module")));
        QVERIFY(!QDir(QString::fromStdString(res->string())).exists());
    }

    void moduleCacheDirCreate()
    {
        qputenv("SYNTALOS_MODULE_TYPE_ID", "devel.datasource");
        const auto res = moduleCacheDir();
        QVERIFY2(res.has_value(), res.error_or("").c_str());
        QCOMPARE(
            QString::fromStdString(res->string()),
            m_tmpDir.filePath(QStringLiteral("xdg-data/Syntalos/cache/modules/devel.datasource")));
        QVERIFY(QDir(QString::fromStdString(res->string())).exists());
    }

    void moduleCacheDirNoTypeId()
    {
        qunsetenv("SYNTALOS_MODULE_TYPE_ID");
        QVERIFY(!moduleCacheDir(false).has_value());
    }

    void moduleCacheDirInvalidId()
    {
        for (const auto id : {"", ".", "..", "../escape", "a/b", "a\\b"}) {
            qputenv("SYNTALOS_MODULE_TYPE_ID", id);
            QVERIFY2(!moduleCacheDir().has_value(), id);
        }
        QVERIFY(!QDir(m_tmpDir.filePath(QStringLiteral("xdg-data/Syntalos/cache/escape"))).exists());
    }

    void moduleCacheDirFlatpak()
    {
        qputenv("SYNTALOS_MODULE_TYPE_ID", "my-module");
        qputenv("container", "flatpak");
        const auto res = moduleCacheDir(false);
        qunsetenv("container");

        QVERIFY2(res.has_value(), res.error_or("").c_str());
        QCOMPARE(
            QString::fromStdString(res->string()),
            m_tmpDir.filePath(QStringLiteral(".var/app/org.syntalos.syntalos/data/cache/modules/my-module")));
    }

    void moduleDataDirFromEnv()
    {
        const auto modDir = m_tmpDir.filePath(QStringLiteral("modules/my-module"));
        QVERIFY(QDir().mkpath(modDir));

        // non-canonical paths are resolved
        qputenv("SYNTALOS_MODULE_DIR", m_tmpDir.filePath(QStringLiteral("modules/../modules/my-module")).toUtf8());
        const auto res = moduleDataDir();
        qunsetenv("SYNTALOS_MODULE_DIR");

        QVERIFY2(res.has_value(), res.error_or("").c_str());
        QCOMPARE(QString::fromStdString(res->string()), QFileInfo(modDir).canonicalFilePath());
    }

    void moduleDataDirExeFallback()
    {
        qunsetenv("SYNTALOS_MODULE_DIR");
        const auto res = moduleDataDir();
        QVERIFY2(res.has_value(), res.error_or("").c_str());
        QCOMPARE(
            QString::fromStdString(res->string()),
            QFileInfo(QCoreApplication::applicationDirPath()).canonicalFilePath());
    }

    void moduleDataDirInvalid()
    {
        qputenv("SYNTALOS_MODULE_DIR", m_tmpDir.filePath(QStringLiteral("does-not-exist")).toUtf8());
        QVERIFY(!moduleDataDir().has_value());

        QFile file(m_tmpDir.filePath(QStringLiteral("regular-file")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        qputenv("SYNTALOS_MODULE_DIR", file.fileName().toUtf8());
        QVERIFY(!moduleDataDir().has_value());

        qunsetenv("SYNTALOS_MODULE_DIR");
    }

    void postToLinkThread()
    {
        // a link does not need a running Syntalos to wait for data, only a module instance ID
        const auto instanceId = QStringLiteral("test-post-%1").arg(QCoreApplication::applicationPid());
        qputenv("SYNTALOS_MODULE_ID", instanceId.toUtf8());
        auto slink = initSyntalosModuleLink();
        qunsetenv("SYNTALOS_MODULE_ID");

        // only touched in the link thread
        std::vector<int> order;
        bool allInLinkThread = true;
        const auto linkThreadId = std::this_thread::get_id();
        auto record = [&](int value) {
            order.push_back(value);
            allInLinkThread = allInLinkThread && (std::this_thread::get_id() == linkThreadId);
        };

        // Wait timeouts are long, so a missing wakeup shows up as a slow test.
        constexpr int waitTimeoutUsec = 2 * 1000 * 1000;
        QElapsedTimer timer;

        // posted before anyone waits, picked up by the first wait
        slink->postToLinkThread([&] {
            record(0);
        });
        timer.start();
        slink->awaitData(waitTimeoutUsec);
        QCOMPARE(order.size(), 1U);
        QVERIFY2(timer.elapsed() < 1000, "Function posted before waiting did not wake up the link thread.");

        // posted from another thread while the link thread is blocked waiting
        std::atomic_bool posted = false;
        std::thread poster([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            for (int i = 1; i <= 3; i++)
                slink->postToLinkThread([&record, i] {
                    record(i);
                });
            posted = true;
        });
        timer.restart();
        while (order.size() < 4 && timer.elapsed() < 10 * 1000)
            slink->awaitData(waitTimeoutUsec);
        const auto elapsedMsec = timer.elapsed();
        poster.join();
        QVERIFY(posted);

        QVERIFY(order == (std::vector<int>{0, 1, 2, 3}));
        QVERIFY2(allInLinkThread, "A posted function ran outside of the link thread.");
        QVERIFY2(
            elapsedMsec < 1000,
            qPrintable(QStringLiteral("Posted functions took %1 ms to run, the link thread was not woken up.")
                           .arg(elapsedMsec)));
    }

private:
    QTemporaryDir m_tmpDir;
};

QTEST_GUILESS_MAIN(TestMLink)
#include "test-mlink.moc"
