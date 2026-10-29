#include <QTest>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include "video/shader_manager.h"
#include "config/settings_manager.h"

class TestShaders : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void initTestCase() {
        // Keep test files out of the developer's real ~/.config
        QStandardPaths::setTestModeEnabled(true);
        Omnipot::ShaderManager::instance().initialize();
    }

    void testDefaultShadersExist() {
        auto &sm = Omnipot::ShaderManager::instance();
        const QStringList available = sm.availableShaders();

        QVERIFY(available.contains(QStringLiteral("Invert")));
        QVERIFY(available.contains(QStringLiteral("Grayscale")));
        QVERIFY(available.contains(QStringLiteral("Sharpen")));
        QVERIFY(available.contains(QStringLiteral("Contour")));
        QVERIFY(available.contains(QStringLiteral("NightVision")));
        QVERIFY(available.contains(QStringLiteral("Emboss")));
        QVERIFY(available.contains(QStringLiteral("Sepia")));
    }

    void testShaderTogglingAndClear() {
        auto &sm = Omnipot::ShaderManager::instance();
        sm.clearActiveShaders(nullptr);

        QVERIFY(sm.activeShaders().isEmpty());
        QCOMPARE(sm.isShaderActive(QStringLiteral("Invert")), false);

        // Toggle Invert
        sm.toggleShader(QStringLiteral("Invert"), nullptr);
        QCOMPARE(sm.isShaderActive(QStringLiteral("Invert")), true);
        QVERIFY(sm.activeShaders().contains(QStringLiteral("Invert")));

        // Toggle Sharpen
        sm.setShaderActive(QStringLiteral("Sharpen"), true, nullptr);
        QCOMPARE(sm.isShaderActive(QStringLiteral("Sharpen")), true);
        QCOMPARE(sm.activeShaders().size(), 2);

        // Untoggle Invert
        sm.toggleShader(QStringLiteral("Invert"), nullptr);
        QCOMPARE(sm.isShaderActive(QStringLiteral("Invert")), false);
        QCOMPARE(sm.activeShaders().size(), 1);

        // Clear all
        sm.clearActiveShaders(nullptr);
        QVERIFY(sm.activeShaders().isEmpty());
    }

    void testShaderFileContent() {
        QString shaderDir = Omnipot::SettingsManager::configDirectory() + QStringLiteral("/shaders");
        QFile file(shaderDir + QStringLiteral("/Invert.glsl"));
        QVERIFY(file.exists());
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));

        QString content = QString::fromUtf8(file.readAll());
        file.close();

        QVERIFY(content.contains(QStringLiteral("//!HOOK MAIN")));
        QVERIFY(content.contains(QStringLiteral("vec4 hook()")));
    }
};

QTEST_MAIN(TestShaders)
#include "test_shaders.moc"
