#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <functional>
#include <mpv/client.h>

namespace Omnipot {

struct SubtitleSearchResult {
    QString language;
    QString provider;
    QString file;
    QString format;
    QString downloadUrl;
    QString webUrl;
    bool checked = false;
};

class SubtitleSearchDialog : public QDialog {
    Q_OBJECT

public:
    explicit SubtitleSearchDialog(mpv_handle *mpv, const QString &currentMediaTitle, QWidget *parent = nullptr);
    ~SubtitleSearchDialog() override = default;

    static QString cleanMediaTitle(const QString &rawTitle);
    static QByteArray decompressGzip(const QByteArray &data);

private Q_SLOTS:
    void searchByTitle();
    void onSearchFinished(QNetworkReply *reply);
    void browseLocalSubtitle();
    void loadSelectedSubtitle();
    void saveSelectedSubtitleAs();
    void openSubtitleWebpage();
    void addCheckedSubtitles();
    void saveCheckedSubtitles();
    void resendRequest();
    void openSettings();

private:
    void setupUi();
    void populateResultsTable(const QList<SubtitleSearchResult> &results);
    void loadSubtitleIntoMpv(const QString &subtitlePath, bool select = true);
    void downloadSubtitle(const SubtitleSearchResult &sub, std::function<void(const QString &filePath, bool ok)> onComplete);

    mpv_handle *m_mpv = nullptr;
    QString m_mediaTitle;
    QNetworkAccessManager *m_netManager = nullptr;

    QLineEdit *m_queryEdit = nullptr;
    QPushButton *m_browseBtn = nullptr;
    QPushButton *m_searchBtn = nullptr;

    QTableWidget *m_resultsTable = nullptr;
    QLabel *m_statusLabel = nullptr;

    QPushButton *m_loadSelectedBtn = nullptr;
    QPushButton *m_saveSelectedBtn = nullptr;
    QPushButton *m_openWebBtn = nullptr;
    QPushButton *m_addCheckedBtn = nullptr;
    QPushButton *m_saveCheckedBtn = nullptr;
    QPushButton *m_resendBtn = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    QPushButton *m_closeBtn = nullptr;

    QList<SubtitleSearchResult> m_currentResults;
};

} // namespace Omnipot
