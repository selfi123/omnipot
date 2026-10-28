#include "ui/preferences_dialog.h"
#include "config/settings_manager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <mpv/client.h>

namespace PotPlayer {

PreferencesDialog::PreferencesDialog(PlaybackEngine *engine, QWidget *parent)
    : QDialog(parent)
    , m_engine(engine)
{
    setWindowTitle(QStringLiteral("Preferences (F5) - PotPlayer Linux"));
    resize(740, 500);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1e1e1e; color: #e0e0e0; font-family: 'Segoe UI', 'Ubuntu', sans-serif; font-size: 12px; }"
        "QTreeWidget { background-color: #181818; color: #cccccc; border: 1px solid #333333; selection-background-color: #383838; selection-color: #f0a020; font-weight: bold; }"
        "QTreeWidget::item { padding: 8px 4px; border-bottom: 1px solid #222222; }"
        "QGroupBox { color: #f0a020; font-weight: bold; border: 1px solid #333333; margin-top: 10px; padding-top: 15px; border-radius: 4px; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 5px; }"
        "QLabel { color: #cccccc; }"
        "QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox { background-color: #2b2b2b; color: #ffffff; border: 1px solid #3c3c3c; border-radius: 3px; padding: 4px; }"
        "QCheckBox { color: #dddddd; spacing: 6px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; background-color: #2b2b2b; border: 1px solid #444444; border-radius: 2px; }"
        "QCheckBox::indicator:checked { background-color: #f0a020; border-color: #f0a020; }"
        "QPushButton { background-color: #333333; color: #ffffff; border: 1px solid #444444; border-radius: 3px; padding: 5px 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #444444; border-color: #f0a020; }"
        "QPushButton:pressed { background-color: #222222; }"
        "QTextEdit { background-color: #151515; color: #00dd66; border: 1px solid #333333; font-family: monospace; font-size: 11px; }"
    ));

    setupUi();
    loadCurrentSettings();
}

void PreferencesDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    // Left navigation tree
    m_treeNav = new QTreeWidget(splitter);
    m_treeNav->setHeaderHidden(true);
    m_treeNav->setFixedWidth(160);

    auto *itemGen = new QTreeWidgetItem(m_treeNav, {QStringLiteral("General")});
    new QTreeWidgetItem(m_treeNav, {QStringLiteral("Playback")});
    new QTreeWidgetItem(m_treeNav, {QStringLiteral("Video")});
    new QTreeWidgetItem(m_treeNav, {QStringLiteral("Audio")});
    new QTreeWidgetItem(m_treeNav, {QStringLiteral("Subtitles")});
    new QTreeWidgetItem(m_treeNav, {QStringLiteral("Diagnostics")});

    splitter->addWidget(m_treeNav);

    // Right stack
    m_stack = new QStackedWidget(splitter);
    m_stack->addWidget(createGeneralPage());      // 0
    m_stack->addWidget(createPlaybackPage());     // 1
    m_stack->addWidget(createVideoPage());        // 2
    m_stack->addWidget(createAudioPage());        // 3
    m_stack->addWidget(createSubtitlePage());     // 4
    m_stack->addWidget(createDiagnosticsPage());  // 5

    splitter->addWidget(m_stack);
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter, 1);

    connect(m_treeNav, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *cur, QTreeWidgetItem*) {
        if (cur) {
            onCategoryChanged(m_treeNav->indexOfTopLevelItem(cur));
        }
    });
    m_treeNav->setCurrentItem(itemGen);

    // Bottom action bar
    auto *bottomLayout = new QHBoxLayout();

    m_resetBtn = new QPushButton(QStringLiteral("Reset Defaults"), this);
    connect(m_resetBtn, &QPushButton::clicked, this, &PreferencesDialog::resetDefaults);
    bottomLayout->addWidget(m_resetBtn);

    m_exportBtn = new QPushButton(QStringLiteral("Export..."), this);
    connect(m_exportBtn, &QPushButton::clicked, this, &PreferencesDialog::exportPresets);
    bottomLayout->addWidget(m_exportBtn);

    m_importBtn = new QPushButton(QStringLiteral("Import..."), this);
    connect(m_importBtn, &QPushButton::clicked, this, &PreferencesDialog::importPresets);
    bottomLayout->addWidget(m_importBtn);

    bottomLayout->addStretch();

    m_okBtn = new QPushButton(QStringLiteral("OK"), this);
    m_okBtn->setStyleSheet(QStringLiteral("background-color: #2e6030; border-color: #408844;"));
    connect(m_okBtn, &QPushButton::clicked, this, [this]() {
        applySettings();
        accept();
    });
    bottomLayout->addWidget(m_okBtn);

    m_cancelBtn = new QPushButton(QStringLiteral("Cancel"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    bottomLayout->addWidget(m_cancelBtn);

    m_applyBtn = new QPushButton(QStringLiteral("Apply"), this);
    connect(m_applyBtn, &QPushButton::clicked, this, &PreferencesDialog::applySettings);
    bottomLayout->addWidget(m_applyBtn);

    mainLayout->addLayout(bottomLayout);
}

void PreferencesDialog::onCategoryChanged(int index) {
    if (index >= 0 && index < m_stack->count()) {
        m_stack->setCurrentIndex(index);
    }
}

QWidget* PreferencesDialog::createGeneralPage() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(QStringLiteral("General Behavior"), page);
    auto *gl = new QVBoxLayout(group);
    gl->setSpacing(10);

    m_rememberPosCheck = new QCheckBox(QStringLiteral("Remember playback position on exit"), group);
    gl->addWidget(m_rememberPosCheck);

    m_singleInstanceCheck = new QCheckBox(QStringLiteral("Enforce single-instance mode (forward CLI media to existing player)"), group);
    gl->addWidget(m_singleInstanceCheck);

    layout->addWidget(group);
    layout->addStretch();
    return page;
}

QWidget* PreferencesDialog::createPlaybackPage() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(QStringLiteral("Playback Options"), page);
    auto *gl = new QGridLayout(group);
    gl->setSpacing(10);

    gl->addWidget(new QLabel(QStringLiteral("Default Speed:"), group), 0, 0);
    m_speedSpin = new QDoubleSpinBox(group);
    m_speedSpin->setRange(0.2, 8.0);
    m_speedSpin->setSingleStep(0.1);
    m_speedSpin->setValue(1.0);
    gl->addWidget(m_speedSpin, 0, 1);

    gl->addWidget(new QLabel(QStringLiteral("Repeat Mode:"), group), 1, 0);
    m_repeatCombo = new QComboBox(group);
    m_repeatCombo->addItems({QStringLiteral("Repeat Off"), QStringLiteral("Repeat Current Track"), QStringLiteral("Repeat All Tracks")});
    gl->addWidget(m_repeatCombo, 1, 1);

    m_shuffleCheck = new QCheckBox(QStringLiteral("Shuffle playlist playback order"), group);
    gl->addWidget(m_shuffleCheck, 2, 0, 1, 2);

    layout->addWidget(group);
    layout->addStretch();
    return page;
}

QWidget* PreferencesDialog::createVideoPage() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(QStringLiteral("Video & GPU Rendering"), page);
    auto *gl = new QGridLayout(group);
    gl->setSpacing(10);

    gl->addWidget(new QLabel(QStringLiteral("Hardware Acceleration:"), group), 0, 0);
    m_hwdecCombo = new QComboBox(group);
    m_hwdecCombo->addItems({
        QStringLiteral("auto-safe (Recommended)"),
        QStringLiteral("vaapi (Intel / AMD GPU)"),
        QStringLiteral("nvdec (NVIDIA GPU)"),
        QStringLiteral("no (Software Decoding)")
    });
    gl->addWidget(m_hwdecCombo, 0, 1);

    gl->addWidget(new QLabel(QStringLiteral("Scaling Algorithm:"), group), 1, 0);
    m_scalerCombo = new QComboBox(group);
    m_scalerCombo->addItems({
        QStringLiteral("spline36 (High Quality Spline)"),
        QStringLiteral("lanczos (Lanczos 3-tap)"),
        QStringLiteral("bicubic (Standard Bicubic)"),
        QStringLiteral("bilinear (Fast Bilinear)")
    });
    gl->addWidget(m_scalerCombo, 1, 1);

    m_deinterlaceCheck = new QCheckBox(QStringLiteral("Enable hardware-adaptive deinterlacing (BwDif)"), group);
    gl->addWidget(m_deinterlaceCheck, 2, 0, 1, 2);

    layout->addWidget(group);
    layout->addStretch();
    return page;
}

QWidget* PreferencesDialog::createAudioPage() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(QStringLiteral("Audio Processing"), page);
    auto *gl = new QGridLayout(group);
    gl->setSpacing(10);

    gl->addWidget(new QLabel(QStringLiteral("Channels Output:"), group), 0, 0);
    m_channelsCombo = new QComboBox(group);
    m_channelsCombo->addItems({
        QStringLiteral("stereo (2.0 Channel Downmix)"),
        QStringLiteral("5.1 (Surround Sound)"),
        QStringLiteral("7.1 (Cinema Surround)"),
        QStringLiteral("auto-safe (Match Source)")
    });
    gl->addWidget(m_channelsCombo, 0, 1);

    m_loudnormCheck = new QCheckBox(QStringLiteral("Enable Audio Normalization / Dynamic Range Compression (EBU R128)"), group);
    gl->addWidget(m_loudnormCheck, 1, 0, 1, 2);

    layout->addWidget(group);
    layout->addStretch();
    return page;
}

QWidget* PreferencesDialog::createSubtitlePage() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(QStringLiteral("Subtitle Appearance"), page);
    auto *gl = new QGridLayout(group);
    gl->setSpacing(10);

    gl->addWidget(new QLabel(QStringLiteral("Font Size:"), group), 0, 0);
    m_fontSizeSpin = new QSpinBox(group);
    m_fontSizeSpin->setRange(12, 120);
    m_fontSizeSpin->setValue(55);
    gl->addWidget(m_fontSizeSpin, 0, 1);

    gl->addWidget(new QLabel(QStringLiteral("Vertical Position:"), group), 1, 0);
    m_subPosSlider = new QSlider(Qt::Horizontal, group);
    m_subPosSlider->setRange(0, 100);
    m_subPosSlider->setValue(100);
    gl->addWidget(m_subPosSlider, 1, 1);

    m_assOverrideCheck = new QCheckBox(QStringLiteral("Override embedded ASS/SSA author styles with player styles"), group);
    gl->addWidget(m_assOverrideCheck, 2, 0, 1, 2);

    layout->addWidget(group);
    layout->addStretch();
    return page;
}

QWidget* PreferencesDialog::createDiagnosticsPage() {
    auto *page = new QWidget();
    auto *layout = new QVBoxLayout(page);

    auto *group = new QGroupBox(QStringLiteral("Engine Diagnostics & Log Output"), page);
    auto *gl = new QVBoxLayout(group);

    m_logEdit = new QTextEdit(group);
    m_logEdit->setReadOnly(true);

    if (m_engine) {
        auto stats = m_engine->streamStats();
        QString text = QStringLiteral(
            "[System] Native PotPlayer Linux Platform Engine initialized.\n"
            "[Engine] libmpv dynamic link verified.\n"
            "[Video Codec] %1\n"
            "[Audio Codec] %2\n"
            "[Resolution] %3 x %4\n"
            "[FPS] %5\n"
            "[HWDEC Mode] %6\n"
            "[Bitrate] Video: %7 kbps | Audio: %8 kbps\n"
            "[A-V Sync Difference] %9 ms\n"
            "[Dropped Frames] %10\n"
        ).arg(stats.videoCodec.isEmpty() ? QStringLiteral("Idle / None") : stats.videoCodec)
         .arg(stats.audioCodec.isEmpty() ? QStringLiteral("Idle / None") : stats.audioCodec)
         .arg(stats.videoWidth)
         .arg(stats.videoHeight)
         .arg(stats.fps, 0, 'f', 2)
         .arg(stats.hwdec)
         .arg(stats.videoBitrate / 1000)
         .arg(stats.audioBitrate / 1000)
         .arg(stats.avSyncDiff * 1000.0, 0, 'f', 1)
         .arg(stats.droppedFrames);
        m_logEdit->setText(text);
    }

    gl->addWidget(m_logEdit);
    layout->addWidget(group);
    return page;
}

void PreferencesDialog::loadCurrentSettings() {
    auto &s = SettingsManager::instance();

    m_rememberPosCheck->setChecked(s.rememberPlaybackPosition());
    m_singleInstanceCheck->setChecked(true);

    m_speedSpin->setValue(s.speed());
    m_repeatCombo->setCurrentIndex(s.repeatMode());
    m_shuffleCheck->setChecked(s.isShuffle());

    QString hw = s.hwdec();
    if (hw == QStringLiteral("vaapi")) m_hwdecCombo->setCurrentIndex(1);
    else if (hw == QStringLiteral("nvdec")) m_hwdecCombo->setCurrentIndex(2);
    else if (hw == QStringLiteral("no")) m_hwdecCombo->setCurrentIndex(3);
    else m_hwdecCombo->setCurrentIndex(0);

    QString sc = s.videoScaler();
    if (sc == QStringLiteral("lanczos")) m_scalerCombo->setCurrentIndex(1);
    else if (sc == QStringLiteral("bicubic")) m_scalerCombo->setCurrentIndex(2);
    else if (sc == QStringLiteral("bilinear")) m_scalerCombo->setCurrentIndex(3);
    else m_scalerCombo->setCurrentIndex(0);

    m_deinterlaceCheck->setChecked(m_engine ? m_engine->isDeinterlacing() : false);

    QString ch = s.audioChannels();
    if (ch == QStringLiteral("5.1")) m_channelsCombo->setCurrentIndex(1);
    else if (ch == QStringLiteral("7.1")) m_channelsCombo->setCurrentIndex(2);
    else if (ch == QStringLiteral("auto-safe")) m_channelsCombo->setCurrentIndex(3);
    else m_channelsCombo->setCurrentIndex(0);

    m_loudnormCheck->setChecked(s.isAudioNormalization());

    if (m_engine) {
        m_fontSizeSpin->setValue(m_engine->subtitleFontSize());
        m_subPosSlider->setValue(m_engine->subtitlePosition());
        m_assOverrideCheck->setChecked(m_engine->isAssOverride());
    }
}

void PreferencesDialog::applySettings() {
    auto &s = SettingsManager::instance();

    s.setRememberPlaybackPosition(m_rememberPosCheck->isChecked());

    double spd = m_speedSpin->value();
    s.setSpeed(spd);
    if (m_engine) m_engine->setSpeed(spd);

    s.setRepeatMode(m_repeatCombo->currentIndex());
    s.setShuffle(m_shuffleCheck->isChecked());

    QString hw = QStringLiteral("auto-safe");
    if (m_hwdecCombo->currentIndex() == 1) hw = QStringLiteral("vaapi");
    else if (m_hwdecCombo->currentIndex() == 2) hw = QStringLiteral("nvdec");
    else if (m_hwdecCombo->currentIndex() == 3) hw = QStringLiteral("no");
    s.setHwdec(hw);
    if (m_engine) m_engine->setHwdec(hw);

    QString sc = QStringLiteral("spline36");
    if (m_scalerCombo->currentIndex() == 1) sc = QStringLiteral("lanczos");
    else if (m_scalerCombo->currentIndex() == 2) sc = QStringLiteral("bicubic");
    else if (m_scalerCombo->currentIndex() == 3) sc = QStringLiteral("bilinear");
    s.setVideoScaler(sc);

    if (m_engine && m_engine->rawMpvHandle()) {
        mpv_set_property_string(m_engine->rawMpvHandle(), "scale", sc.toUtf8().constData());
    }

    bool deint = m_deinterlaceCheck->isChecked();
    if (m_engine) m_engine->setDeinterlacing(deint);

    QString ch = QStringLiteral("stereo");
    if (m_channelsCombo->currentIndex() == 1) ch = QStringLiteral("5.1");
    else if (m_channelsCombo->currentIndex() == 2) ch = QStringLiteral("7.1");
    else if (m_channelsCombo->currentIndex() == 3) ch = QStringLiteral("auto-safe");
    s.setAudioChannels(ch);

    if (m_engine && m_engine->rawMpvHandle()) {
        mpv_set_property_string(m_engine->rawMpvHandle(), "audio-channels", ch.toUtf8().constData());
    }

    bool loudnorm = m_loudnormCheck->isChecked();
    s.setAudioNormalization(loudnorm);
    if (m_engine) {
        m_engine->setNormalizerEnabled(loudnorm);
    }

    if (m_engine) {
        m_engine->setSubtitleFontSize(m_fontSizeSpin->value());
        m_engine->setSubtitlePosition(m_subPosSlider->value());
        m_engine->setAssOverride(m_assOverrideCheck->isChecked());
    }

    s.sync();
}

void PreferencesDialog::resetDefaults() {
    auto reply = QMessageBox::question(
        this,
        QStringLiteral("Reset Settings"),
        QStringLiteral("Are you sure you want to restore all PotPlayer settings to default?"),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        SettingsManager::instance().resetToDefaults();
        loadCurrentSettings();
        applySettings();
    }
}

void PreferencesDialog::exportPresets() {
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export Presets"), QString(), QStringLiteral("INI Config (*.ini)"));
    if (!path.isEmpty()) {
        if (SettingsManager::instance().exportSettings(path)) {
            QMessageBox::information(this, QStringLiteral("Export Success"), QStringLiteral("Settings exported successfully."));
        } else {
            QMessageBox::warning(this, QStringLiteral("Export Failed"), QStringLiteral("Could not export settings to the chosen file."));
        }
    }
}

void PreferencesDialog::importPresets() {
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Import Presets"), QString(), QStringLiteral("INI Config (*.ini)"));
    if (!path.isEmpty()) {
        if (SettingsManager::instance().importSettings(path)) {
            loadCurrentSettings();
            applySettings();
            QMessageBox::information(this, QStringLiteral("Import Success"), QStringLiteral("Settings imported successfully."));
        } else {
            QMessageBox::warning(this, QStringLiteral("Import Failed"), QStringLiteral("Could not import settings from file."));
        }
    }
}

} // namespace PotPlayer
