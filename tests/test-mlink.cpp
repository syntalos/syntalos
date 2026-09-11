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

private:
    QTemporaryDir m_tmpDir;
};

QTEST_GUILESS_MAIN(TestMLink)
#include "test-mlink.moc"
