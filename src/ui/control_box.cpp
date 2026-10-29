#include "ui/control_box.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QKeyEvent>
#include <QSignalBlocker>
#include <vector>

namespace Omnipot {

ControlBoxDialog::ControlBoxDialog(PlaybackEngine *engine, QWidget *parent)
    : QDialog(parent, Qt::Tool | Qt::WindowCloseButtonHint)
    , m_engine(engine)
{
    setWindowTitle(QStringLiteral("Control - Omnipot"));
    setFixedWidth(380);
    setupUi();

    connect(m_engine, &PlaybackEngine::speedChanged, this, &ControlBoxDialog::syncFromEngine);
    connect(m_engine, &PlaybackEngine::colorsChanged, this, &ControlBoxDialog::syncFromEngine);
    connect(m_engine, &PlaybackEngine::equalizerChanged, this, &ControlBoxDialog::syncFromEngine);
    connect(m_engine, &PlaybackEngine::tracksChanged, this, &ControlBoxDialog::syncFromEngine);

    syncFromEngine();
}

void ControlBoxDialog::setupUi() {
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1A1A1A; color: #CCCCCC; font-size: 11px; }"
        "QTabWidget::pane { border: 1px solid #333333; background-color: #1E1E1E; top: -1px; }"
        "QTabBar::tab { background: #252525; color: #AAAAAA; padding: 6px 12px; border: 1px solid #333333; border-bottom: none; border-top-left-radius: 3px; border-top-right-radius: 3px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #1E1E1E; color: #FFFFFF; border-bottom: 1px solid #1E1E1E; }"
        "QTabBar::tab:hover:!selected { background: #2E2E2E; }"
        "QGroupBox { border: 1px solid #333333; border-radius: 3px; margin-top: 8px; padding-top: 10px; font-weight: bold; color: #E0E0E0; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QPushButton { background-color: #2A2A2A; border: 1px solid #3C3C3C; border-radius: 2px; padding: 4px 8px; color: #DDDDDD; min-height: 20px; }"
        "QPushButton:hover { background-color: #383838; border-color: #555555; }"
        "QPushButton:pressed { background-color: #151515; }"
        "QComboBox { background-color: #242424; border: 1px solid #383838; border-radius: 2px; padding: 3px 6px; color: #EEEEEE; min-height: 20px; }"
        "QComboBox QAbstractItemView { background-color: #222222; border: 1px solid #444444; selection-background-color: #CC2222; color: #FFFFFF; }"
        "QSlider::groove:horizontal { height: 4px; background: #333333; border-radius: 2px; }"
        "QSlider::sub-page:horizontal { background: #CC2222; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #EEEEEE; border: 1px solid #777777; width: 12px; margin-top: -4px; margin-bottom: -4px; border-radius: 6px; }"
        "QSlider::handle:horizontal:hover { background: #FFFFFF; border-color: #CC2222; }"
        "QSlider::groove:vertical { width: 4px; background: #333333; border-radius: 2px; }"
        "QSlider::add-page:vertical { background: #CC2222; border-radius: 2px; }"
        "QSlider::handle:vertical { background: #EEEEEE; border: 1px solid #777777; height: 12px; margin-left: -4px; margin-right: -4px; border-radius: 6px; }"
        "QCheckBox { color: #CCCCCC; spacing: 6px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; background-color: #242424; border: 1px solid #444444; border-radius: 2px; }"
        "QCheckBox::indicator:checked { background-color: #CC2222; border-color: #EE3333; }"
    ));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(createPlaybackTab(), QStringLiteral("Playback"));
    tabs->addTab(createVideoTab(), QStringLiteral("Video"));
    tabs->addTab(createAudioTab(), QStringLiteral("Audio"));
    tabs->addTab(createSubtitleTab(), QStringLiteral("Subtitle"));
    tabs->addTab(createColorTab(), QStringLiteral("Color"));

    mainLayout->addWidget(tabs);
}

QWidget* ControlBoxDialog::createPlaybackTab() {
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Speed Group
    auto *speedBox = new QGroupBox(QStringLiteral("Playback Speed"), tab);
    auto *speedLayout = new QHBoxLayout(speedBox);
    auto *sDown = new QPushButton(QStringLiteral("- 0.1x (X)"), speedBox);
    connect(sDown, &QPushButton::clicked, this, [this]() { m_engine->setSpeed(m_engine->speed() - 0.1); });
    speedLayout->addWidget(sDown);

    m_speedLabel = new QLabel(QStringLiteral("1.00x"), speedBox);
    m_speedLabel->setAlignment(Qt::AlignCenter);
    m_speedLabel->setStyleSheet(QStringLiteral("font-weight: bold; color: #00E5FF;"));
    speedLayout->addWidget(m_speedLabel);

    auto *sReset = new QPushButton(QStringLiteral("1.0x (Z)"), speedBox);
    connect(sReset, &QPushButton::clicked, this, [this]() { m_engine->setSpeed(1.0); });
    speedLayout->addWidget(sReset);

    auto *sUp = new QPushButton(QStringLiteral("+ 0.1x (C)"), speedBox);
    connect(sUp, &QPushButton::clicked, this, [this]() { m_engine->setSpeed(m_engine->speed() + 0.1); });
    speedLayout->addWidget(sUp);

    layout->addWidget(speedBox);

    // A-B Repeat Group
    auto *loopBox = new QGroupBox(QStringLiteral("A-B Repeat Looping"), tab);
    auto *loopLayout = new QVBoxLayout(loopBox);

    m_loopStatusLabel = new QLabel(QStringLiteral("Repeat: Disabled"), loopBox);
    m_loopStatusLabel->setAlignment(Qt::AlignCenter);
    loopLayout->addWidget(m_loopStatusLabel);

    auto *loopBtns = new QHBoxLayout();
    auto *setABtn = new QPushButton(QStringLiteral("Set A ([)"), loopBox);
    connect(setABtn, &QPushButton::clicked, this, [this]() { m_engine->setLoopA(m_engine->position()); syncFromEngine(); });
    loopBtns->addWidget(setABtn);

    auto *setBBtn = new QPushButton(QStringLiteral("Set B (])"), loopBox);
    connect(setBBtn, &QPushButton::clicked, this, [this]() { m_engine->setLoopB(m_engine->position()); syncFromEngine(); });
    loopBtns->addWidget(setBBtn);

    auto *clearLoopBtn = new QPushButton(QStringLiteral("Clear (\\)"), loopBox);
    connect(clearLoopBtn, &QPushButton::clicked, this, [this]() { m_engine->clearLoop(); syncFromEngine(); });
    loopBtns->addWidget(clearLoopBtn);

    loopLayout->addLayout(loopBtns);
    layout->addWidget(loopBox);

    // Frame Stepping Group
    auto *stepBox = new QGroupBox(QStringLiteral("Frame Stepping"), tab);
    auto *stepLayout = new QHBoxLayout(stepBox);
    auto *stepBack = new QPushButton(QStringLiteral("◀ Previous Frame (D)"), stepBox);
    connect(stepBack, &QPushButton::clicked, m_engine, &PlaybackEngine::frameBackStep);
    stepLayout->addWidget(stepBack);

    auto *stepFwd = new QPushButton(QStringLiteral("Next Frame (F) ▶"), stepBox);
    connect(stepFwd, &QPushButton::clicked, m_engine, &PlaybackEngine::frameStep);
    stepLayout->addWidget(stepFwd);

    layout->addWidget(stepBox);
    layout->addStretch();
    return tab;
}

QWidget* ControlBoxDialog::createVideoTab() {
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Hardware Decoding
    auto *hwBox = new QGroupBox(QStringLiteral("Hardware Acceleration (GPU)"), tab);
    auto *hwLayout = new QHBoxLayout(hwBox);
    m_hwdecCombo = new QComboBox(hwBox);
    m_hwdecCombo->addItem(QStringLiteral("Auto (Safe / Recommended)"), QStringLiteral("auto-safe"));
    m_hwdecCombo->addItem(QStringLiteral("VA-API (Intel / AMD GPU)"), QStringLiteral("vaapi"));
    m_hwdecCombo->addItem(QStringLiteral("NVDEC (NVIDIA CUDA GPU)"), QStringLiteral("nvdec"));
    m_hwdecCombo->addItem(QStringLiteral("Disabled (Software CPU Decoding)"), QStringLiteral("no"));
    connect(m_hwdecCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        m_engine->setHwdec(m_hwdecCombo->itemData(idx).toString());
    });
    hwLayout->addWidget(m_hwdecCombo);
    layout->addWidget(hwBox);

    // Aspect Ratio
    auto *arBox = new QGroupBox(QStringLiteral("Aspect Ratio"), tab);
    auto *arLayout = new QHBoxLayout(arBox);
    m_aspectCombo = new QComboBox(arBox);
    m_aspectCombo->addItem(QStringLiteral("Original Ratio"), QStringLiteral("-1"));
    m_aspectCombo->addItem(QStringLiteral("4:3 Standard"), QStringLiteral("4:3"));
    m_aspectCombo->addItem(QStringLiteral("16:9 Widescreen"), QStringLiteral("16:9"));
    m_aspectCombo->addItem(QStringLiteral("16:10 Computer"), QStringLiteral("16:10"));
    m_aspectCombo->addItem(QStringLiteral("2.35:1 Cinematic"), QStringLiteral("2.35:1"));
    connect(m_aspectCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        m_engine->setAspectRatio(m_aspectCombo->itemData(idx).toString());
    });
    arLayout->addWidget(m_aspectCombo);
    layout->addWidget(arBox);

    // Transforms
    auto *trBox = new QGroupBox(QStringLiteral("Video Transformations"), tab);
    auto *trLayout = new QVBoxLayout(trBox);

    auto *rotBtn = new QPushButton(QStringLiteral("Rotate 90° Clockwise"), trBox);
    connect(rotBtn, &QPushButton::clicked, m_engine, &PlaybackEngine::rotateClockwise);
    trLayout->addWidget(rotBtn);

    m_hflipCheck = new QCheckBox(QStringLiteral("Horizontal Flip (Mirror)"), trBox);
    connect(m_hflipCheck, &QCheckBox::toggled, m_engine, &PlaybackEngine::setHorizontalFlip);
    trLayout->addWidget(m_hflipCheck);

    m_vflipCheck = new QCheckBox(QStringLiteral("Vertical Flip (Upside Down)"), trBox);
    connect(m_vflipCheck, &QCheckBox::toggled, m_engine, &PlaybackEngine::setVerticalFlip);
    trLayout->addWidget(m_vflipCheck);

    m_deinterlaceCheck = new QCheckBox(QStringLiteral("Deinterlacing Filter (BwDif)"), trBox);
    connect(m_deinterlaceCheck, &QCheckBox::toggled, m_engine, &PlaybackEngine::setDeinterlacing);
    trLayout->addWidget(m_deinterlaceCheck);

    layout->addWidget(trBox);
    layout->addStretch();
    return tab;
}

QWidget* ControlBoxDialog::createAudioTab() {
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // Equalizer Header
    auto *eqTop = new QHBoxLayout();
    m_eqCheck = new QCheckBox(QStringLiteral("Enable 10-Band Equalizer"), tab);
    connect(m_eqCheck, &QCheckBox::toggled, m_engine, &PlaybackEngine::setEqualizerEnabled);
    eqTop->addWidget(m_eqCheck);

    m_eqPresetCombo = new QComboBox(tab);
    m_eqPresetCombo->addItems({QStringLiteral("Flat"), QStringLiteral("Rock"), QStringLiteral("Pop"),
                               QStringLiteral("Classical"), QStringLiteral("Jazz"), QStringLiteral("Club"), QStringLiteral("Vocal")});
    connect(m_eqPresetCombo, &QComboBox::currentTextChanged, m_engine, &PlaybackEngine::setEqualizerPreset);
    eqTop->addWidget(m_eqPresetCombo);

    auto *resetEqBtn = new QPushButton(QStringLiteral("Reset"), tab);
    connect(resetEqBtn, &QPushButton::clicked, m_engine, &PlaybackEngine::resetEqualizer);
    eqTop->addWidget(resetEqBtn);
    layout->addLayout(eqTop);

    // Sliders
    auto *slidersBox = new QGroupBox(QStringLiteral("Frequencies (-12dB to +12dB)"), tab);
    auto *slidersLayout = new QHBoxLayout(slidersBox);
    slidersLayout->setContentsMargins(4, 8, 4, 4);

    static const QStringList freqNames = {
        QStringLiteral("31"), QStringLiteral("62"), QStringLiteral("125"), QStringLiteral("250"), QStringLiteral("500"),
        QStringLiteral("1k"), QStringLiteral("2k"), QStringLiteral("4k"), QStringLiteral("8k"), QStringLiteral("16k")
    };

    for (int i = 0; i < 10; ++i) {
        auto *col = new QVBoxLayout();
        col->setAlignment(Qt::AlignCenter);

        auto *slider = new QSlider(Qt::Vertical, slidersBox);
        slider->setRange(-120, 120); // 0.1 dB increments (-12.0 to +12.0)
        slider->setValue(0);
        slider->setFixedHeight(100);
        m_eqSliders[i] = slider;

        connect(slider, &QSlider::valueChanged, this, [this, i](int val) {
            m_engine->setEqualizerBand(i, val / 10.0);
        });

        col->addWidget(slider);

        auto *lbl = new QLabel(freqNames[i], slidersBox);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QStringLiteral("font-size: 9px; color: #888888;"));
        m_eqLabels[i] = lbl;
        col->addWidget(lbl);

        slidersLayout->addLayout(col);
    }
    layout->addWidget(slidersBox);

    // Audio Delay
    auto *delayBox = new QGroupBox(QStringLiteral("Audio Delay / Sync"), tab);
    auto *delayLayout = new QHBoxLayout(delayBox);
    auto *dDown = new QPushButton(QStringLiteral("- 50ms (Shift+<)"), delayBox);
    connect(dDown, &QPushButton::clicked, this, [this]() {
        m_engine->setAudioDelay(m_engine->audioDelay() - 0.05);
        syncFromEngine();
    });
    delayLayout->addWidget(dDown);

    m_audioDelayLabel = new QLabel(QStringLiteral("0.00 s"), delayBox);
    m_audioDelayLabel->setAlignment(Qt::AlignCenter);
    delayLayout->addWidget(m_audioDelayLabel);

    auto *dReset = new QPushButton(QStringLiteral("0 ms"), delayBox);
    connect(dReset, &QPushButton::clicked, this, [this]() {
        m_engine->setAudioDelay(0.0);
        syncFromEngine();
    });
    delayLayout->addWidget(dReset);

    auto *dUp = new QPushButton(QStringLiteral("+ 50ms (Shift+>)"), delayBox);
    connect(dUp, &QPushButton::clicked, this, [this]() {
        m_engine->setAudioDelay(m_engine->audioDelay() + 0.05);
        syncFromEngine();
    });
    delayLayout->addWidget(dUp);

    layout->addWidget(delayBox);

    auto *normBox = new QGroupBox(QStringLiteral("Dynamic Range Normalizer"), tab);
    auto *normLayout = new QHBoxLayout(normBox);
    auto *normCheck = new QCheckBox(QStringLiteral("Enable Normalizer (Boost Quiet Dialogue - Shift+N)"), normBox);
    normCheck->setChecked(m_engine->isNormalizerEnabled());
    connect(normCheck, &QCheckBox::toggled, m_engine, &PlaybackEngine::setNormalizerEnabled);
    normLayout->addWidget(normCheck);
    layout->addWidget(normBox);

    layout->addStretch();
    return tab;
}

QWidget* ControlBoxDialog::createSubtitleTab() {
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Stream Selector
    auto *stBox = new QGroupBox(QStringLiteral("Subtitle Track"), tab);
    auto *stLayout = new QHBoxLayout(stBox);
    m_subTrackCombo = new QComboBox(stBox);
    connect(m_subTrackCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (idx >= 0) {
            int trackId = m_subTrackCombo->itemData(idx).toInt();
            m_engine->setSubtitleTrack(trackId);
        }
    });
    stLayout->addWidget(m_subTrackCombo);
    layout->addWidget(stBox);

    // Delay Group
    auto *delBox = new QGroupBox(QStringLiteral("Subtitle Sync / Delay"), tab);
    auto *delLayout = new QHBoxLayout(delBox);
    auto *sdDown = new QPushButton(QStringLiteral("- 0.5s (<)"), delBox);
    connect(sdDown, &QPushButton::clicked, this, [this]() {
        m_engine->setSubtitleDelay(m_engine->subtitleDelay() - 0.5);
        syncFromEngine();
    });
    delLayout->addWidget(sdDown);

    m_subDelayLabel = new QLabel(QStringLiteral("0.00 s"), delBox);
    m_subDelayLabel->setAlignment(Qt::AlignCenter);
    delLayout->addWidget(m_subDelayLabel);

    auto *sdReset = new QPushButton(QStringLiteral("0.0s"), delBox);
    connect(sdReset, &QPushButton::clicked, this, [this]() {
        m_engine->setSubtitleDelay(0.0);
        syncFromEngine();
    });
    delLayout->addWidget(sdReset);

    auto *sdUp = new QPushButton(QStringLiteral("+ 0.5s (>)"), delBox);
    connect(sdUp, &QPushButton::clicked, this, [this]() {
        m_engine->setSubtitleDelay(m_engine->subtitleDelay() + 0.5);
        syncFromEngine();
    });
    delLayout->addWidget(sdUp);
    layout->addWidget(delBox);

    // Styling
    auto *styleBox = new QGroupBox(QStringLiteral("Subtitle Position & Styling"), tab);
    auto *styleLayout = new QGridLayout(styleBox);

    styleLayout->addWidget(new QLabel(QStringLiteral("Font Size:"), styleBox), 0, 0);
    m_subSizeSlider = new QSlider(Qt::Horizontal, styleBox);
    m_subSizeSlider->setRange(15, 95);
    m_subSizeSlider->setValue(55);
    connect(m_subSizeSlider, &QSlider::valueChanged, this, [this](int val) {
        m_engine->setSubtitleFontSize(val);
        m_subSizeLabel->setText(QString::number(val));
    });
    styleLayout->addWidget(m_subSizeSlider, 0, 1);
    m_subSizeLabel = new QLabel(QStringLiteral("55"), styleBox);
    styleLayout->addWidget(m_subSizeLabel, 0, 2);

    styleLayout->addWidget(new QLabel(QStringLiteral("Position V:"), styleBox), 1, 0);
    m_subPosSlider = new QSlider(Qt::Horizontal, styleBox);
    m_subPosSlider->setRange(0, 100);
    m_subPosSlider->setValue(100);
    connect(m_subPosSlider, &QSlider::valueChanged, this, [this](int val) {
        m_engine->setSubtitlePosition(val);
        m_subPosLabel->setText(QString::number(val));
    });
    styleLayout->addWidget(m_subPosSlider, 1, 1);
    m_subPosLabel = new QLabel(QStringLiteral("100"), styleBox);
    styleLayout->addWidget(m_subPosLabel, 1, 2);

    m_assOverrideCheck = new QCheckBox(QStringLiteral("Force Default Font on ASS/SSA styles"), styleBox);
    connect(m_assOverrideCheck, &QCheckBox::toggled, m_engine, &PlaybackEngine::setAssOverride);
    styleLayout->addWidget(m_assOverrideCheck, 2, 0, 1, 3);

    layout->addWidget(styleBox);
    layout->addStretch();
    return tab;
}

QWidget* ControlBoxDialog::createColorTab() {
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto *procampBox = new QGroupBox(QStringLiteral("Video Color Adjustments (ProcAmp)"), tab);
    auto *pLayout = new QGridLayout(procampBox);

    // Brightness
    pLayout->addWidget(new QLabel(QStringLiteral("Brightness:"), procampBox), 0, 0);
    m_brightnessSlider = new QSlider(Qt::Horizontal, procampBox);
    m_brightnessSlider->setRange(-100, 100);
    m_brightnessSlider->setValue(0);
    connect(m_brightnessSlider, &QSlider::valueChanged, this, [this](int v) {
        m_engine->setBrightness(v);
        m_brightnessVal->setText(QString::number(v));
    });
    pLayout->addWidget(m_brightnessSlider, 0, 1);
    m_brightnessVal = new QLabel(QStringLiteral("0"), procampBox);
    m_brightnessVal->setFixedWidth(30);
    pLayout->addWidget(m_brightnessVal, 0, 2);

    // Contrast
    pLayout->addWidget(new QLabel(QStringLiteral("Contrast:"), procampBox), 1, 0);
    m_contrastSlider = new QSlider(Qt::Horizontal, procampBox);
    m_contrastSlider->setRange(-100, 100);
    m_contrastSlider->setValue(0);
    connect(m_contrastSlider, &QSlider::valueChanged, this, [this](int v) {
        m_engine->setContrast(v);
        m_contrastVal->setText(QString::number(v));
    });
    pLayout->addWidget(m_contrastSlider, 1, 1);
    m_contrastVal = new QLabel(QStringLiteral("0"), procampBox);
    m_contrastVal->setFixedWidth(30);
    pLayout->addWidget(m_contrastVal, 1, 2);

    // Saturation
    pLayout->addWidget(new QLabel(QStringLiteral("Saturation:"), procampBox), 2, 0);
    m_saturationSlider = new QSlider(Qt::Horizontal, procampBox);
    m_saturationSlider->setRange(-100, 100);
    m_saturationSlider->setValue(0);
    connect(m_saturationSlider, &QSlider::valueChanged, this, [this](int v) {
        m_engine->setSaturation(v);
        m_saturationVal->setText(QString::number(v));
    });
    pLayout->addWidget(m_saturationSlider, 2, 1);
    m_saturationVal = new QLabel(QStringLiteral("0"), procampBox);
    m_saturationVal->setFixedWidth(30);
    pLayout->addWidget(m_saturationVal, 2, 2);

    // Hue
    pLayout->addWidget(new QLabel(QStringLiteral("Hue:"), procampBox), 3, 0);
    m_hueSlider = new QSlider(Qt::Horizontal, procampBox);
    m_hueSlider->setRange(-100, 100);
    m_hueSlider->setValue(0);
    connect(m_hueSlider, &QSlider::valueChanged, this, [this](int v) {
        m_engine->setHue(v);
        m_hueVal->setText(QString::number(v));
    });
    pLayout->addWidget(m_hueSlider, 3, 1);
    m_hueVal = new QLabel(QStringLiteral("0"), procampBox);
    m_hueVal->setFixedWidth(30);
    pLayout->addWidget(m_hueVal, 3, 2);

    layout->addWidget(procampBox);

    auto *resetBtn = new QPushButton(QStringLiteral("Reset All Colors to Default"), tab);
    connect(resetBtn, &QPushButton::clicked, m_engine, &PlaybackEngine::resetColors);
    layout->addWidget(resetBtn);

    layout->addStretch();
    return tab;
}

void ControlBoxDialog::syncFromEngine() {
    if (!m_engine) return;

    // Reflect engine state without feeding it back: every slider/checkbox signal would call
    // the engine again, and each equalizer change rebuilds mpv's audio filter chain (audible gap)
    std::vector<QSignalBlocker> blockers;
    for (QObject *w : std::initializer_list<QObject*>{
             m_hflipCheck, m_vflipCheck, m_deinterlaceCheck, m_eqCheck, m_subSizeSlider, m_subPosSlider,
             m_assOverrideCheck, m_brightnessSlider, m_contrastSlider, m_saturationSlider, m_hueSlider}) {
        blockers.emplace_back(w);
    }
    for (auto *slider : m_eqSliders) {
        blockers.emplace_back(slider);
    }

    // Speed
    m_speedLabel->setText(QStringLiteral("%1x").arg(m_engine->speed(), 0, 'f', 2));

    // A-B loop
    double a = m_engine->loopA();
    double b = m_engine->loopB();
    if (a >= 0.0 && b >= 0.0) {
        m_loopStatusLabel->setText(QStringLiteral("Repeat [%1 - %2]").arg(a, 0, 'f', 1).arg(b, 0, 'f', 1));
        m_loopStatusLabel->setStyleSheet(QStringLiteral("color: #FF5252; font-weight: bold;"));
    } else if (a >= 0.0) {
        m_loopStatusLabel->setText(QStringLiteral("Point A set at %1s, awaiting B").arg(a, 0, 'f', 1));
        m_loopStatusLabel->setStyleSheet(QStringLiteral("color: #FFD740; font-weight: bold;"));
    } else {
        m_loopStatusLabel->setText(QStringLiteral("Repeat: Disabled"));
        m_loopStatusLabel->setStyleSheet(QStringLiteral("color: #888888;"));
    }

    // Transforms
    m_hflipCheck->setChecked(m_engine->isHorizontalFlip());
    m_vflipCheck->setChecked(m_engine->isVerticalFlip());
    m_deinterlaceCheck->setChecked(m_engine->isDeinterlacing());

    // Audio
    m_eqCheck->setChecked(m_engine->isEqualizerEnabled());
    for (int i = 0; i < 10; ++i) {
        m_eqSliders[i]->setValue(static_cast<int>(m_engine->equalizerBand(i) * 10.0));
    }
    m_audioDelayLabel->setText(QStringLiteral("%1 s").arg(m_engine->audioDelay(), 0, 'f', 2));

    // Subtitles
    m_subDelayLabel->setText(QStringLiteral("%1 s").arg(m_engine->subtitleDelay(), 0, 'f', 2));
    m_subSizeSlider->setValue(m_engine->subtitleFontSize());
    m_subSizeLabel->setText(QString::number(m_engine->subtitleFontSize()));
    m_subPosSlider->setValue(m_engine->subtitlePosition());
    m_subPosLabel->setText(QString::number(m_engine->subtitlePosition()));
    m_assOverrideCheck->setChecked(m_engine->isAssOverride());

    // Subtitle tracks dropdown
    m_subTrackCombo->blockSignals(true);
    m_subTrackCombo->clear();
    m_subTrackCombo->addItem(QStringLiteral("None / Disable"), -1);
    const auto subs = m_engine->subtitleTracks();
    int selectedIdx = 0;
    for (int i = 0; i < subs.size(); ++i) {
        const auto &s = subs[i];
        QString name = QStringLiteral("[%1] %2 (%3)")
            .arg(s.id)
            .arg(s.title.isEmpty() ? QStringLiteral("Track %1").arg(s.id) : s.title)
            .arg(s.language.isEmpty() ? QStringLiteral("UND") : s.language);
        m_subTrackCombo->addItem(name, s.id);
        if (s.isSelected) {
            selectedIdx = i + 1;
        }
    }
    m_subTrackCombo->setCurrentIndex(selectedIdx);
    m_subTrackCombo->blockSignals(false);

    // Colors
    m_brightnessSlider->setValue(m_engine->brightness());
    m_brightnessVal->setText(QString::number(m_engine->brightness()));
    m_contrastSlider->setValue(m_engine->contrast());
    m_contrastVal->setText(QString::number(m_engine->contrast()));
    m_saturationSlider->setValue(m_engine->saturation());
    m_saturationVal->setText(QString::number(m_engine->saturation()));
    m_hueSlider->setValue(m_engine->hue());
    m_hueVal->setText(QString::number(m_engine->hue()));
}

void ControlBoxDialog::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_F6) {
        close();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

} // namespace Omnipot
