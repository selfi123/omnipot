#pragma once

#include <QString>
#include <QStringList>
#include <QMap>
#include <mpv/client.h>

namespace Omnipot {

class ShaderManager {
public:
    static ShaderManager& instance();

    void initialize();
    QStringList availableShaders() const;
    QStringList activeShaders() const { return m_activeShaders; }
    bool isShaderActive(const QString &shaderName) const;
    void toggleShader(const QString &shaderName, mpv_handle *mpv);
    void setShaderActive(const QString &shaderName, bool active, mpv_handle *mpv);
    void clearActiveShaders(mpv_handle *mpv);

private:
    ShaderManager() = default;
    void applyToMpv(mpv_handle *mpv);
    void writeShaderFiles();

    QString m_shaderDir;
    QMap<QString, QString> m_shaderFiles; // name -> filepath
    QStringList m_activeShaders;
};

} // namespace Omnipot
