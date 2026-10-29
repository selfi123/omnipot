#include "video/shader_manager.h"
#include "config/settings_manager.h"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

namespace Omnipot {

ShaderManager& ShaderManager::instance() {
    static ShaderManager inst;
    return inst;
}

void ShaderManager::initialize() {
    m_shaderDir = SettingsManager::configDirectory() + QStringLiteral("/shaders");
    QDir dir(m_shaderDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    writeShaderFiles();

    // Enumerate all .glsl shaders
    m_shaderFiles.clear();
    const QStringList files = dir.entryList({QStringLiteral("*.glsl")}, QDir::Files, QDir::Name);
    for (const QString &file : files) {
        QString name = QFileInfo(file).completeBaseName();
        m_shaderFiles.insert(name, dir.absoluteFilePath(file));
    }
}

QStringList ShaderManager::availableShaders() const {
    return m_shaderFiles.keys();
}

bool ShaderManager::isShaderActive(const QString &shaderName) const {
    return m_activeShaders.contains(shaderName);
}

void ShaderManager::toggleShader(const QString &shaderName, mpv_handle *mpv) {
    if (m_activeShaders.contains(shaderName)) {
        m_activeShaders.removeAll(shaderName);
    } else if (m_shaderFiles.contains(shaderName)) {
        m_activeShaders.append(shaderName);
    }
    applyToMpv(mpv);
}

void ShaderManager::setShaderActive(const QString &shaderName, bool active, mpv_handle *mpv) {
    if (active) {
        if (!m_activeShaders.contains(shaderName) && m_shaderFiles.contains(shaderName)) {
            m_activeShaders.append(shaderName);
        }
    } else {
        m_activeShaders.removeAll(shaderName);
    }
    applyToMpv(mpv);
}

void ShaderManager::clearActiveShaders(mpv_handle *mpv) {
    m_activeShaders.clear();
    applyToMpv(mpv);
}

void ShaderManager::applyToMpv(mpv_handle *mpv) {
    if (!mpv) {
        return;
    }

    QStringList paths;
    for (const QString &shaderName : m_activeShaders) {
        if (m_shaderFiles.contains(shaderName)) {
            paths.append(m_shaderFiles.value(shaderName));
        }
    }

    QString shaderString = paths.join(QStringLiteral(":"));
    int status = mpv_set_property_string(mpv, "glsl-shaders", shaderString.toUtf8().constData());
    if (status < 0) {
        qWarning() << "[ShaderManager] Failed to apply glsl-shaders to mpv:" << mpv_error_string(status);
    } else {
        qDebug() << "[ShaderManager] Applied glsl-shaders:" << (shaderString.isEmpty() ? "none" : shaderString);
    }
}

void ShaderManager::writeShaderFiles() {
    struct DefaultShader {
        const char *filename;
        const char *code;
    };

    static const DefaultShader defaultShaders[] = {
        {
            "Invert.glsl",
            "//!HOOK MAIN\n"
            "//!BIND HOOKED\n"
            "//!DESC Color Invert (Negative)\n\n"
            "vec4 hook() {\n"
            "    vec4 c = HOOKED_tex(HOOKED_pos);\n"
            "    return vec4(1.0 - c.rgb, c.a);\n"
            "}\n"
        },
        {
            "Grayscale.glsl",
            "//!HOOK MAIN\n"
            "//!BIND HOOKED\n"
            "//!DESC Grayscale (Luminance)\n\n"
            "vec4 hook() {\n"
            "    vec4 c = HOOKED_tex(HOOKED_pos);\n"
            "    float gray = dot(c.rgb, vec3(0.299, 0.587, 0.114));\n"
            "    return vec4(vec3(gray), c.a);\n"
            "}\n"
        },
        {
            "Sharpen.glsl",
            "//!HOOK MAIN\n"
            "//!BIND HOOKED\n"
            "//!DESC Sharpen (3x3 Matrix)\n\n"
            "vec4 hook() {\n"
            "    vec2 pt = HOOKED_pt;\n"
            "    vec4 c = HOOKED_tex(HOOKED_pos);\n"
            "    vec4 up    = HOOKED_tex(HOOKED_pos + vec2(0.0, -pt.y));\n"
            "    vec4 down  = HOOKED_tex(HOOKED_pos + vec2(0.0,  pt.y));\n"
            "    vec4 left  = HOOKED_tex(HOOKED_pos + vec2(-pt.x, 0.0));\n"
            "    vec4 right = HOOKED_tex(HOOKED_pos + vec2( pt.x, 0.0));\n"
            "    vec3 sharp = c.rgb * 5.0 - (up.rgb + down.rgb + left.rgb + right.rgb);\n"
            "    return vec4(clamp(sharp, 0.0, 1.0), c.a);\n"
            "}\n"
        },
        {
            "Contour.glsl",
            "//!HOOK MAIN\n"
            "//!BIND HOOKED\n"
            "//!DESC Edge Contour\n\n"
            "vec4 hook() {\n"
            "    vec2 pt = HOOKED_pt;\n"
            "    vec4 c = HOOKED_tex(HOOKED_pos);\n"
            "    vec4 up    = HOOKED_tex(HOOKED_pos + vec2(0.0, -pt.y));\n"
            "    vec4 down  = HOOKED_tex(HOOKED_pos + vec2(0.0,  pt.y));\n"
            "    vec4 left  = HOOKED_tex(HOOKED_pos + vec2(-pt.x, 0.0));\n"
            "    vec4 right = HOOKED_tex(HOOKED_pos + vec2( pt.x, 0.0));\n"
            "    vec3 edge = abs(up.rgb - down.rgb) + abs(left.rgb - right.rgb);\n"
            "    float val = dot(edge, vec3(0.333));\n"
            "    return vec4(vec3(clamp(val * 4.0, 0.0, 1.0)), c.a);\n"
            "}\n"
        },
        {
            "NightVision.glsl",
            "//!HOOK MAIN\n"
            "//!BIND HOOKED\n"
            "//!DESC Night Vision Phosphor\n\n"
            "vec4 hook() {\n"
            "    vec4 c = HOOKED_tex(HOOKED_pos);\n"
            "    float lum = dot(c.rgb, vec3(0.299, 0.587, 0.114));\n"
            "    lum = clamp(lum * 1.5, 0.0, 1.0);\n"
            "    return vec4(lum * 0.2, lum * 1.2, lum * 0.2, c.a);\n"
            "}\n"
        },
        {
            "Emboss.glsl",
            "//!HOOK MAIN\n"
            "//!BIND HOOKED\n"
            "//!DESC Emboss 3D Relief\n\n"
            "vec4 hook() {\n"
            "    vec2 pt = HOOKED_pt;\n"
            "    vec4 c1 = HOOKED_tex(HOOKED_pos - pt);\n"
            "    vec4 c2 = HOOKED_tex(HOOKED_pos + pt);\n"
            "    vec3 diff = (c1.rgb - c2.rgb) * 2.0 + vec3(0.5);\n"
            "    return vec4(clamp(diff, 0.0, 1.0), 1.0);\n"
            "}\n"
        },
        {
            "Sepia.glsl",
            "//!HOOK MAIN\n"
            "//!BIND HOOKED\n"
            "//!DESC Sepia Tone\n\n"
            "vec4 hook() {\n"
            "    vec4 c = HOOKED_tex(HOOKED_pos);\n"
            "    float r = dot(c.rgb, vec3(0.393, 0.769, 0.189));\n"
            "    float g = dot(c.rgb, vec3(0.349, 0.686, 0.168));\n"
            "    float b = dot(c.rgb, vec3(0.272, 0.534, 0.131));\n"
            "    return vec4(clamp(vec3(r, g, b), 0.0, 1.0), c.a);\n"
            "}\n"
        }
    };

    for (const auto &item : defaultShaders) {
        QString filePath = m_shaderDir + QStringLiteral("/") + QString::fromLatin1(item.filename);
        QFile file(filePath);
        if (!file.exists()) {
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << item.code;
                file.close();
            }
        }
    }
}

} // namespace Omnipot
