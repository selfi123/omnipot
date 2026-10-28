#pragma once

#include <QDialog>
#include <QTreeWidget>
#include <QStackedWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include "playback/playback_engine.h"

namespace PotPlayer {

class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(PlaybackEngine *engine, QWidget *parent = nullptr);
    ~PreferencesDialog() override = default;

private Q_SLOTS:
    void applySettings();
    void resetDefaults();
    void exportPresets();
    void importPresets();
    void onCategoryChanged(int index);

private:
    void setupUi();
    QWidget* createGeneralPage();
    QWidget* createPlaybackPage();
    QWidget* createVideoPage();
    QWidget* createAudioPage();
    QWidget* createSubtitlePage();
    QWidget* createDiagnosticsPage();
    void loadCurrentSettings();

    PlaybackEngine *m_engine = nullptr;

    QTreeWidget *m_treeNav = nullptr;
    QStackedWidget *m_stack = nullptr;

    // General
    QCheckBox *m_rememberPosCheck = nullptr;
    QCheckBox *m_singleInstanceCheck = nullptr;

    // Playback
    QDoubleSpinBox *m_speedSpin = nullptr;
    QComboBox *m_repeatCombo = nullptr;
    QCheckBox *m_shuffleCheck = nullptr;

    // Video
    QComboBox *m_hwdecCombo = nullptr;
    QComboBox *m_scalerCombo = nullptr;
    QCheckBox *m_deinterlaceCheck = nullptr;

    // Audio
    QComboBox *m_channelsCombo = nullptr;
    QCheckBox *m_loudnormCheck = nullptr;

    // Subtitles
    QSpinBox *m_fontSizeSpin = nullptr;
    QSlider *m_subPosSlider = nullptr;
    QCheckBox *m_assOverrideCheck = nullptr;

    // Diagnostics / Log
    QTextEdit *m_logEdit = nullptr;

    QPushButton *m_resetBtn = nullptr;
    QPushButton *m_exportBtn = nullptr;
    QPushButton *m_importBtn = nullptr;
    QPushButton *m_applyBtn = nullptr;
    QPushButton *m_okBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
};

} // namespace PotPlayer
