#include "ui/playlist_drawer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMenu>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QHeaderView>
#include <QToolTip>
#include <QFontMetrics>

namespace Omnipot {

PlaylistDrawer::PlaylistDrawer(PlaylistManager *playlistManager, QWidget *parent)
    : QWidget(parent)
    , m_manager(playlistManager)
{
    setFixedWidth(280);
    setAcceptDrops(true);
    setupUi();

    connect(m_manager, &PlaylistManager::playlistChanged, this, &PlaylistDrawer::updateHeader);
    connect(m_manager, &PlaylistManager::currentItemChanged, this, &PlaylistDrawer::updateHeader);
    connect(m_manager, &PlaylistManager::playbackModeChanged, this, &PlaylistDrawer::syncModeButtons);
    syncModeButtons();
}

void PlaylistDrawer::setupUi() {
    setStyleSheet(QStringLiteral(
        "QWidget { background-color: #1A1A1A; color: #CCCCCC; font-size: 11px; }"
        "QLineEdit { background-color: #242424; border: 1px solid #333333; border-radius: 2px; padding: 3px 6px; color: #FFFFFF; }"
        "QLineEdit:focus { border-color: #CC2222; }"
        "QListView { background-color: #141414; border: 1px solid #282828; color: #DDDDDD; selection-background-color: #383838; selection-color: #FFFFFF; outline: none; }"
        "QListView::item { padding: 4px 6px; border-bottom: 1px solid #1E1E1E; }"
        "QListView::item:hover { background-color: #262626; }"
        "QPushButton { background-color: #252525; border: 1px solid #383838; border-radius: 2px; padding: 3px 8px; min-height: 18px; color: #DDDDDD; }"
        "QPushButton:hover { background-color: #353535; border-color: #555555; }"
        "QPushButton:pressed { background-color: #181818; }"
        "QPushButton:checked { background-color: #4A1A1A; border-color: #AA2222; color: #FFFFFF; }"
    ));

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(4);

    // Header row
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("Search playlist..."));
    m_searchEdit->setToolTip(QStringLiteral("Search playlist items"));
    connect(m_searchEdit, &QLineEdit::textChanged, this, &PlaylistDrawer::onSearchTextChanged);
    headerLayout->addWidget(m_searchEdit, 1);

    auto *closeBtn = new QPushButton(QStringLiteral("✕"), this);
    closeBtn->setFixedWidth(24);
    closeBtn->setToolTip(QStringLiteral("Close Playlist (F8)"));
    connect(closeBtn, &QPushButton::clicked, this, &PlaylistDrawer::closeRequested);
    headerLayout->addWidget(closeBtn);

    rootLayout->addLayout(headerLayout);

    // Proxy model for instant search
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_manager);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterRole(PlaylistManager::TitleRole);

    // List view
    m_listView = new QListView(this);
    m_listView->setModel(m_proxyModel);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_listView, &QListView::doubleClicked, this, &PlaylistDrawer::onItemDoubleClicked);
    rootLayout->addWidget(m_listView, 1);

    // Toolbar row 1: Playback modes & Operations
    auto *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(4);

    m_addBtn = new QPushButton(QStringLiteral("+ Add"), this);
    m_addBtn->setToolTip(QStringLiteral("Add Files or Folder to Playlist"));
    auto *addMenu = new QMenu(m_addBtn);
    addMenu->addAction(QStringLiteral("Add File(s)..."), this, &PlaylistDrawer::promptAddFiles);
    addMenu->addAction(QStringLiteral("Add Folder..."), this, &PlaylistDrawer::promptAddFolder);
    m_addBtn->setMenu(addMenu);
    toolbarLayout->addWidget(m_addBtn);

    m_removeBtn = new QPushButton(QStringLiteral("- Del"), this);
    m_removeBtn->setToolTip(QStringLiteral("Remove selected item from playlist (Delete)"));
    connect(m_removeBtn, &QPushButton::clicked, this, &PlaylistDrawer::removeSelected);
    toolbarLayout->addWidget(m_removeBtn);

    m_clearBtn = new QPushButton(QStringLiteral("Clear"), this);
    m_clearBtn->setToolTip(QStringLiteral("Clear entire playlist"));
    connect(m_clearBtn, &QPushButton::clicked, m_manager, &PlaylistManager::clear);
    toolbarLayout->addWidget(m_clearBtn);

    rootLayout->addLayout(toolbarLayout);

    // Toolbar row 2: Modes & Save/Load
    auto *bottomToolbar = new QHBoxLayout();
    bottomToolbar->setContentsMargins(0, 0, 0, 0);
    bottomToolbar->setSpacing(4);

    m_shuffleBtn = new QPushButton(QStringLiteral("🔀"), this);
    m_shuffleBtn->setCheckable(true);
    m_shuffleBtn->setToolTip(QStringLiteral("Toggle Shuffle playback"));
    connect(m_shuffleBtn, &QPushButton::clicked, this, &PlaylistDrawer::toggleShuffle);
    bottomToolbar->addWidget(m_shuffleBtn);

    m_repeatBtn = new QPushButton(QStringLiteral("🔁 Off"), this);
    m_repeatBtn->setToolTip(QStringLiteral("Cycle Repeat Mode (Off / One / All)"));
    connect(m_repeatBtn, &QPushButton::clicked, this, &PlaylistDrawer::cycleRepeatMode);
    bottomToolbar->addWidget(m_repeatBtn);

    m_saveLoadBtn = new QPushButton(QStringLiteral("💾 Menu"), this);
    m_saveLoadBtn->setToolTip(QStringLiteral("Save or Load Playlist (.dpl / .m3u)"));
    auto *saveLoadMenu = new QMenu(m_saveLoadBtn);
    saveLoadMenu->addAction(QStringLiteral("Open Playlist (.dpl, .m3u)..."), this, &PlaylistDrawer::promptLoadPlaylist);
    saveLoadMenu->addAction(QStringLiteral("Save Playlist As (.dpl, .m3u)..."), this, &PlaylistDrawer::promptSavePlaylist);
    m_saveLoadBtn->setMenu(saveLoadMenu);
    bottomToolbar->addWidget(m_saveLoadBtn);

    for (auto *w : {static_cast<QWidget*>(m_searchEdit), static_cast<QWidget*>(closeBtn),
                    static_cast<QWidget*>(m_addBtn), static_cast<QWidget*>(m_removeBtn),
                    static_cast<QWidget*>(m_clearBtn), static_cast<QWidget*>(m_shuffleBtn),
                    static_cast<QWidget*>(m_repeatBtn), static_cast<QWidget*>(m_saveLoadBtn)}) {
        w->installEventFilter(this);
    }

    rootLayout->addLayout(bottomToolbar);
}

void PlaylistDrawer::updateHeader() {
    // Scrolls to current playing item if valid
    int curr = m_manager->currentIndex();
    if (curr >= 0) {
        QModelIndex srcIdx = m_manager->index(curr, 0);
        QModelIndex proxyIdx = m_proxyModel->mapFromSource(srcIdx);
        if (proxyIdx.isValid()) {
            m_listView->scrollTo(proxyIdx);
        }
    }
}

void PlaylistDrawer::onSearchTextChanged(const QString &text) {
    m_proxyModel->setFilterFixedString(text);
}

void PlaylistDrawer::onItemDoubleClicked(const QModelIndex &proxyIndex) {
    if (!proxyIndex.isValid()) return;
    QModelIndex srcIndex = m_proxyModel->mapToSource(proxyIndex);
    if (srcIndex.isValid()) {
        m_manager->setCurrentIndex(srcIndex.row());
    }
}

void PlaylistDrawer::promptAddFiles() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("Add Media Files to Playlist"),
        QString(),
        QStringLiteral("Media Files (*.mp4 *.mkv *.avi *.webm *.flv *.ts *.mp3 *.flac *.aac *.ogg *.opus *.wav);;All Files (*)")
    );
    for (const QString &f : files) {
        m_manager->addPath(f);
    }
}

void PlaylistDrawer::promptAddFolder() {
    const QString folder = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Add Media Folder to Playlist")
    );
    if (!folder.isEmpty()) {
        m_manager->addFolder(folder, true);
    }
}

void PlaylistDrawer::removeSelected() {
    QModelIndex proxyIndex = m_listView->currentIndex();
    if (proxyIndex.isValid()) {
        QModelIndex srcIndex = m_proxyModel->mapToSource(proxyIndex);
        if (srcIndex.isValid()) {
            m_manager->removeItem(srcIndex.row());
        }
    }
}

void PlaylistDrawer::toggleShuffle() {
    bool shuf = m_shuffleBtn->isChecked();
    m_manager->setShuffle(shuf);
}

void PlaylistDrawer::cycleRepeatMode() {
    switch (m_manager->repeatMode()) {
    case RepeatMode::None:
        m_manager->setRepeatMode(RepeatMode::Track);
        break;
    case RepeatMode::Track:
        m_manager->setRepeatMode(RepeatMode::All);
        break;
    case RepeatMode::All:
    default:
        m_manager->setRepeatMode(RepeatMode::None);
        break;
    }
}

void PlaylistDrawer::syncModeButtons() {
    // Modes can also change from Preferences, so the buttons follow the manager
    m_shuffleBtn->setChecked(m_manager->isShuffle());
    switch (m_manager->repeatMode()) {
    case RepeatMode::Track:
        m_repeatBtn->setText(QStringLiteral("🔂 Track"));
        break;
    case RepeatMode::All:
        m_repeatBtn->setText(QStringLiteral("🔁 All"));
        break;
    case RepeatMode::None:
    default:
        m_repeatBtn->setText(QStringLiteral("🔁 Off"));
        break;
    }
}

void PlaylistDrawer::promptSavePlaylist() {
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("Save Playlist"),
        QString(),
        QStringLiteral("DPL Playlist (*.dpl);;M3U Playlist (*.m3u);;M3U8 Playlist (*.m3u8)")
    );
    if (!filePath.isEmpty()) {
        m_manager->savePlaylist(filePath);
    }
}

void PlaylistDrawer::promptLoadPlaylist() {
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Open Playlist"),
        QString(),
        QStringLiteral("Playlist Files (*.dpl *.m3u *.m3u8);;All Files (*)")
    );
    if (!filePath.isEmpty()) {
        m_manager->loadPlaylist(filePath);
    }
}

void PlaylistDrawer::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void PlaylistDrawer::dropEvent(QDropEvent *event) {
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const auto &url : urls) {
        QString localPath = url.toLocalFile();
        if (!localPath.isEmpty()) {
            m_manager->addPath(localPath);
        } else {
            m_manager->addPath(url.toString());
        }
    }
    event->acceptProposedAction();
}

bool PlaylistDrawer::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::ToolTip) {
        auto *widget = qobject_cast<QWidget*>(watched);
        if (widget && !widget->toolTip().isEmpty()) {
            QPoint widgetTopCenter = widget->mapToGlobal(QPoint(widget->width() / 2, 0));
            QFontMetrics fm(QToolTip::font());
            QString plainTip = widget->toolTip();
            int tipWidth = fm.horizontalAdvance(plainTip) + 24;
            QPoint tipPos(widgetTopCenter.x() - tipWidth / 2, widgetTopCenter.y() - 34);
            QString richTip = QStringLiteral("<span style='color: #FFFFFF; font-weight: bold;'>%1</span>").arg(plainTip.toHtmlEscaped());
            QToolTip::showText(tipPos, richTip, widget);
            return true;
        }
    } else if (event->type() == QEvent::Leave) {
        QToolTip::hideText();
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace Omnipot
