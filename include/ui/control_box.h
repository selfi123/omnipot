#pragma once

#include <QDialog>
#include <QTabWidget>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include "playback/playback_engine.h"

namespace Omnipot {

class ControlBoxDialog : public QDialog {
    Q_OBJECT

public:
    explicit ControlBoxDialog(PlaybackEngine *engine, QWidget *parent = nullptr);
    ~ControlBoxDialog() override = default;

public Q_SLOTS:
    void syncFromEngine();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setupUi();
    QWidget* createPlaybackTab();
    QWidget* createVideoTab();
    QWidget* createAudioTab();
    QWidget* createSubtitleTab();
    QWidget* createColorTab();

    PlaybackEngine *m_engine = nullptr;

    // Playback Tab Widgets
    QLabel *m_speedLabel = nullptr;
    QLabel *m_loopStatusLabel = nullptr;

    // Video Tab Widgets
    QComboBox *m_hwdecCombo = nullptr;
    QComboBox *m_aspectCombo = nullptr;
    QCheckBox *m_deinterlaceCheck = nullptr;
    QCheckBox *m_hflipCheck = nullptr;
    QCheckBox *m_vflipCheck = nullptr;

    // Audio Tab Widgets
    QCheckBox *m_eqCheck = nullptr;
    QComboBox *m_eqPresetCombo = nullptr;
    std::array<QSlider*, 10> m_eqSliders{};
    std::array<QLabel*, 10> m_eqLabels{};
    QLabel *m_audioDelayLabel = nullptr;

    // Subtitle Tab Widgets
    QComboBox *m_subTrackCombo = nullptr;
    QLabel *m_subDelayLabel = nullptr;
    QSlider *m_subSizeSlider = nullptr;
    QLabel *m_subSizeLabel = nullptr;
    QSlider *m_subPosSlider = nullptr;
    QLabel *m_subPosLabel = nullptr;
    QCheckBox *m_assOverrideCheck = nullptr;

    // Color Tab Widgets
    QSlider *m_brightnessSlider = nullptr;
    QLabel *m_brightnessVal = nullptr;
    QSlider *m_contrastSlider = nullptr;
    QLabel *m_contrastVal = nullptr;
    QSlider *m_saturationSlider = nullptr;
    QLabel *m_saturationVal = nullptr;
    QSlider *m_hueSlider = nullptr;
    QLabel *m_hueVal = nullptr;
};

} // namespace Omnipot
