#pragma once

#include <QWidget>
#include <QListView>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include "playlist/playlist_manager.h"

namespace Omnipot {

class PlaylistDrawer : public QWidget {
    Q_OBJECT

public:
    explicit PlaylistDrawer(PlaylistManager *playlistManager, QWidget *parent = nullptr);
    ~PlaylistDrawer() override = default;

Q_SIGNALS:
    void itemSelected(int originalIndex);
    void closeRequested();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private Q_SLOTS:
    void onSearchTextChanged(const QString &text);
    void onItemDoubleClicked(const QModelIndex &proxyIndex);
    void promptAddFiles();
    void promptAddFolder();
    void removeSelected();
    void toggleShuffle();
    void cycleRepeatMode();
    void promptSavePlaylist();
    void promptLoadPlaylist();

private:
    void setupUi();
    void updateHeader();
    void syncModeButtons();

    PlaylistManager *m_manager = nullptr;
    QSortFilterProxyModel *m_proxyModel = nullptr;

    QLineEdit *m_searchEdit = nullptr;
    QListView *m_listView = nullptr;
    QPushButton *m_shuffleBtn = nullptr;
    QPushButton *m_repeatBtn = nullptr;
    QPushButton *m_addBtn = nullptr;
    QPushButton *m_removeBtn = nullptr;
    QPushButton *m_clearBtn = nullptr;
    QPushButton *m_saveLoadBtn = nullptr;
};

} // namespace Omnipot
