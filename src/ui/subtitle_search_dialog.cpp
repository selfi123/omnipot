#include "ui/subtitle_search_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QDebug>
#include <zlib.h>
#include <cstring>

namespace PotPlayer {

QString SubtitleSearchDialog::cleanMediaTitle(const QString &rawTitle) {
    QString title = QFileInfo(rawTitle).completeBaseName();
    if (title.isEmpty()) title = rawTitle;

    // Replace dots, underscores, dashes with spaces
    static const QRegularExpression punctRegex(QStringLiteral("[._\\-+]+"));
    title.replace(punctRegex, QStringLiteral(" "));

    // Strip common release tags, resolutions, and video codecs
    static const QRegularExpression tagsRegex(
        QStringLiteral("(?i)\\b(19\\d\\d|20\\d\\d|1080p|720p|480p|2160p|4k|uhd|bluray|bdrip|web[\\s\\-]?dl|webrip|web|h[.\\-]?264|x264|h[.\\-]?265|x265|hevc|aac|dts|ddp5[.\\-]1|atmos|remux|hdr|sdr|proper|repack|cinemacity|rarbg|yts|pignus|am|evo|naisu)\\b")
    );
    title.replace(tagsRegex, QStringLiteral(""));

    // Collapse whitespace
    static const QRegularExpression spaceRegex(QStringLiteral("\\s+"));
    title = title.trimmed().replace(spaceRegex, QStringLiteral(" "));
    return title;
}

QByteArray SubtitleSearchDialog::decompressGzip(const QByteArray &data) {
    if (data.isEmpty()) return QByteArray();

    // Check gzip magic bytes 0x1f, 0x8b
    if (data.size() < 2 || static_cast<unsigned char>(data[0]) != 0x1f || static_cast<unsigned char>(data[1]) != 0x8b) {
        return data;
    }

    z_stream strm;
    std::memset(&strm, 0, sizeof(strm));
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    strm.avail_in = static_cast<uInt>(data.size());

    // 16 + MAX_WBITS (31) instructs zlib to decompress gzip streams
    if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) {
        return data;
    }

    QByteArray decompressed;
    char buffer[16384];
    int ret = Z_OK;

    while (ret == Z_OK) {
        strm.next_out = reinterpret_cast<Bytef*>(buffer);
        strm.avail_out = sizeof(buffer);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret < 0 && ret != Z_STREAM_END) {
            inflateEnd(&strm);
            return data; // Return original on decode error
        }
        int have = sizeof(buffer) - strm.avail_out;
        decompressed.append(buffer, have);
    }

    inflateEnd(&strm);
    return decompressed.isEmpty() ? data : decompressed;
}

SubtitleSearchDialog::SubtitleSearchDialog(mpv_handle *mpv, const QString &currentMediaTitle, QWidget *parent)
    : QDialog(parent)
    , m_mpv(mpv)
    , m_mediaTitle(currentMediaTitle)
{
    m_netManager = new QNetworkAccessManager(this);

    setWindowTitle(QStringLiteral("Subtitle Downloader"));
    resize(740, 500);
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1c1c1c; color: #e6e6e6; font-family: 'Segoe UI', 'Ubuntu', sans-serif; font-size: 12px; }"
        "QGroupBox { color: #cccccc; border: 1px solid #383838; margin-top: 10px; padding-top: 14px; border-radius: 3px; font-weight: normal; }"
        "QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top left; padding: 0 4px; left: 10px; }"
        "QLabel { color: #d0d0d0; }"
        "QLineEdit { background-color: #2b2b2b; color: #ffffff; border: 1px solid #444444; border-radius: 2px; padding: 3px 6px; selection-background-color: #f0a020; selection-color: #000000; }"
        "QPushButton { background-color: #2e2e2e; color: #e0e0e0; border: 1px solid #484848; border-radius: 2px; padding: 4px 10px; min-height: 20px; font-size: 11px; }"
        "QPushButton:hover { background-color: #3a3a3a; border-color: #f0a020; color: #ffffff; }"
        "QPushButton:pressed { background-color: #222222; }"
        "QPushButton:disabled { color: #666666; border-color: #333333; }"
        "QTableWidget { background-color: #222222; alternate-background-color: #282828; color: #ffffff; border: 1px solid #383838; gridline-color: #303030; selection-background-color: #f0a020; selection-color: #000000; outline: none; }"
        "QTableWidget::viewport { background-color: #222222; }"
        "QHeaderView::section { background-color: #282828; color: #cccccc; padding: 4px 6px; border: 1px solid #333333; font-weight: normal; }"
        "QScrollBar:vertical { background: #1e1e1e; width: 12px; margin: 0; }"
        "QScrollBar::handle:vertical { background: #3d3d3d; border-radius: 3px; min-height: 20px; margin: 2px; }"
        "QScrollBar::handle:vertical:hover { background: #555555; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { background: #1e1e1e; height: 12px; margin: 0; }"
        "QScrollBar::handle:horizontal { background: #3d3d3d; border-radius: 3px; min-width: 20px; margin: 2px; }"
        "QScrollBar::handle:horizontal:hover { background: #555555; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
    ));

    setupUi();

    QString cleanQuery = cleanMediaTitle(m_mediaTitle);
    if (cleanQuery.isEmpty()) cleanQuery = m_mediaTitle;
    m_queryEdit->setText(cleanQuery);

    if (!cleanQuery.isEmpty()) {
        searchByTitle();
    }
}

void SubtitleSearchDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 8, 12, 10);
    mainLayout->setSpacing(8);

    auto *groupBox = new QGroupBox(QStringLiteral("Available subtitles"), this);
    auto *groupLayout = new QVBoxLayout(groupBox);
    groupLayout->setContentsMargins(10, 12, 10, 10);
    groupLayout->setSpacing(8);

    // Top search bar: Title: [ ... ] [ Open Local Subtitle... ] [ Search Online ]
    auto *searchBarLayout = new QHBoxLayout();
    searchBarLayout->setSpacing(6);

    searchBarLayout->addWidget(new QLabel(QStringLiteral("Title:"), groupBox));

    m_queryEdit = new QLineEdit(groupBox);
    connect(m_queryEdit, &QLineEdit::returnPressed, this, &SubtitleSearchDialog::searchByTitle);
    searchBarLayout->addWidget(m_queryEdit, 1);

    m_browseBtn = new QPushButton(QStringLiteral("📂 Open Local Subtitle..."), groupBox);
    connect(m_browseBtn, &QPushButton::clicked, this, &SubtitleSearchDialog::browseLocalSubtitle);
    searchBarLayout->addWidget(m_browseBtn);

    m_searchBtn = new QPushButton(QStringLiteral("Search Online"), groupBox);
    connect(m_searchBtn, &QPushButton::clicked, this, &SubtitleSearchDialog::searchByTitle);
    searchBarLayout->addWidget(m_searchBtn);

    groupLayout->addLayout(searchBarLayout);

    // Table view: Language, Provider, File, Format
    m_resultsTable = new QTableWidget(groupBox);
    m_resultsTable->setColumnCount(4);
    m_resultsTable->setHorizontalHeaderLabels({
        QStringLiteral("Language"),
        QStringLiteral("Provider"),
        QStringLiteral("File"),
        QStringLiteral("Format")
    });
    m_resultsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_resultsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_resultsTable->setColumnWidth(0, 140);
    m_resultsTable->setColumnWidth(1, 120);
    m_resultsTable->setColumnWidth(3, 70);
    m_resultsTable->verticalHeader()->setVisible(false);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->viewport()->setAutoFillBackground(true);
    connect(m_resultsTable, &QTableWidget::doubleClicked, this, &SubtitleSearchDialog::loadSelectedSubtitle);

    groupLayout->addWidget(m_resultsTable, 1);

    // Row 1 buttons: [ Load Selected Subtitle ] [ Save Selected As... ] [ Open Webpage ]
    auto *row1Layout = new QHBoxLayout();
    row1Layout->setSpacing(6);
    row1Layout->addStretch();

    m_loadSelectedBtn = new QPushButton(QStringLiteral("Load Selected Subtitle"), groupBox);
    connect(m_loadSelectedBtn, &QPushButton::clicked, this, &SubtitleSearchDialog::loadSelectedSubtitle);
    row1Layout->addWidget(m_loadSelectedBtn);

    m_saveSelectedBtn = new QPushButton(QStringLiteral("Save Selected As..."), groupBox);
    connect(m_saveSelectedBtn, &QPushButton::clicked, this, &SubtitleSearchDialog::saveSelectedSubtitleAs);
    row1Layout->addWidget(m_saveSelectedBtn);

    m_openWebBtn = new QPushButton(QStringLiteral("Open Webpage"), groupBox);
    connect(m_openWebBtn, &QPushButton::clicked, this, &SubtitleSearchDialog::openSubtitleWebpage);
    row1Layout->addWidget(m_openWebBtn);

    groupLayout->addLayout(row1Layout);

    // Row 2 buttons: [ Batch Add Checked ] [ Batch Save Checked ] [ Resend Request ] [ Settings... ]
    auto *row2Layout = new QHBoxLayout();
    row2Layout->setSpacing(6);
    row2Layout->addStretch();

    m_addCheckedBtn = new QPushButton(QStringLiteral("Batch Add Checked"), groupBox);
    connect(m_addCheckedBtn, &QPushButton::clicked, this, &SubtitleSearchDialog::addCheckedSubtitles);
    row2Layout->addWidget(m_addCheckedBtn);

    m_saveCheckedBtn = new QPushButton(QStringLiteral("Batch Save Checked"), groupBox);
    connect(m_saveCheckedBtn, &QPushButton::clicked, this, &SubtitleSearchDialog::saveCheckedSubtitles);
    row2Layout->addWidget(m_saveCheckedBtn);

    m_resendBtn = new QPushButton(QStringLiteral("Resend Request"), groupBox);
    connect(m_resendBtn, &QPushButton::clicked, this, &SubtitleSearchDialog::resendRequest);
    row2Layout->addWidget(m_resendBtn);

    m_settingsBtn = new QPushButton(QStringLiteral("Settings..."), groupBox);
    connect(m_settingsBtn, &QPushButton::clicked, this, &SubtitleSearchDialog::openSettings);
    row2Layout->addWidget(m_settingsBtn);

    groupLayout->addLayout(row2Layout);

    mainLayout->addWidget(groupBox, 1);

    // Status label
    m_statusLabel = new QLabel(QStringLiteral("Ready."), this);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px;"));
    mainLayout->addWidget(m_statusLabel);

    // Bottom Center: [ Close ]
    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    m_closeBtn = new QPushButton(QStringLiteral("Close"), this);
    m_closeBtn->setFixedWidth(90);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomLayout->addWidget(m_closeBtn);
    bottomLayout->addStretch();

    mainLayout->addLayout(bottomLayout);
}

void SubtitleSearchDialog::searchByTitle() {
    QString query = m_queryEdit->text().trimmed();
    if (query.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("Please enter a media title to search."));
        return;
    }

    m_statusLabel->setText(QStringLiteral("Searching OpenSubtitles for '%1'...").arg(query));
    m_resultsTable->setRowCount(0);
    m_currentResults.clear();
    m_searchBtn->setEnabled(false);

    // Encode spaces as '+' for OpenSubtitles REST endpoint
    QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(query)).replace(QStringLiteral("%20"), QStringLiteral("+"));
    QUrl url(QStringLiteral("https://rest.opensubtitles.org/search/query-%1").arg(encoded));

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("VLSub 0.10.2"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_netManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onSearchFinished(reply);
    });
}

void SubtitleSearchDialog::onSearchFinished(QNetworkReply *reply) {
    m_searchBtn->setEnabled(true);
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        m_statusLabel->setText(QStringLiteral("Search failed: %1").arg(reply->errorString()));
        return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isArray()) {
        m_statusLabel->setText(QStringLiteral("No subtitles found or unexpected response."));
        return;
    }

    QJsonArray array = doc.array();
    QList<SubtitleSearchResult> results;

    for (const QJsonValue &val : array) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        SubtitleSearchResult res;
        res.language = obj.value(QStringLiteral("LanguageName")).toString();
        if (res.language.isEmpty()) {
            res.language = obj.value(QStringLiteral("SubLanguageID")).toString();
        }
        res.provider = QStringLiteral("OpenSubtitles");
        res.file = obj.value(QStringLiteral("SubFileName")).toString();
        res.format = obj.value(QStringLiteral("SubFormat")).toString(QStringLiteral("srt"));
        res.downloadUrl = obj.value(QStringLiteral("SubDownloadLink")).toString();
        res.webUrl = obj.value(QStringLiteral("SubtitlesLink")).toString();
        res.checked = false;

        if (!res.file.isEmpty() && !res.downloadUrl.isEmpty()) {
            results.append(res);
        }
    }

    if (results.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("No subtitle matches found on OpenSubtitles."));
        return;
    }

    // Auto-check English or first match
    bool checkedAny = false;
    for (auto &res : results) {
        if (res.language.compare(QStringLiteral("English"), Qt::CaseInsensitive) == 0) {
            res.checked = true;
            checkedAny = true;
            break;
        }
    }
    if (!checkedAny && !results.isEmpty()) {
        results[0].checked = true;
    }

    populateResultsTable(results);
    m_statusLabel->setText(QStringLiteral("Received %1 live subtitle matches from OpenSubtitles.").arg(results.size()));
}

void SubtitleSearchDialog::populateResultsTable(const QList<SubtitleSearchResult> &results) {
    m_currentResults = results;
    m_resultsTable->setRowCount(results.size());

    for (int i = 0; i < results.size(); ++i) {
        const auto &res = results[i];

        // Column 0: Language with Checkbox
        auto *langItem = new QTableWidgetItem(res.language);
        langItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        langItem->setCheckState(res.checked ? Qt::Checked : Qt::Unchecked);
        m_resultsTable->setItem(i, 0, langItem);

        // Column 1: Provider
        auto *provItem = new QTableWidgetItem(res.provider);
        m_resultsTable->setItem(i, 1, provItem);

        // Column 2: File
        auto *fileItem = new QTableWidgetItem(res.file);
        m_resultsTable->setItem(i, 2, fileItem);

        // Column 3: Format
        auto *fmtItem = new QTableWidgetItem(res.format);
        m_resultsTable->setItem(i, 3, fmtItem);
    }

    if (!results.isEmpty()) {
        m_resultsTable->selectRow(0);
    }
}

void SubtitleSearchDialog::browseLocalSubtitle() {
    QString filter = QStringLiteral("Subtitle Files (*.srt *.ass *.ssa *.vtt *.sub *.smi *.idx *.sup);;All Files (*.*)");
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open Local Subtitle File"), QString(), filter);
    if (path.isEmpty()) return;

    QFileInfo fi(path);
    SubtitleSearchResult localRes;
    localRes.language = QStringLiteral("Local");
    localRes.provider = QStringLiteral("Local File");
    localRes.file = path;
    localRes.format = fi.suffix().toLower();
    localRes.downloadUrl = QString();
    localRes.webUrl = QString();
    localRes.checked = true;

    m_currentResults.prepend(localRes);
    populateResultsTable(m_currentResults);
    m_resultsTable->selectRow(0);

    loadSubtitleIntoMpv(path, true);
    m_statusLabel->setText(QStringLiteral("Loaded local subtitle: %1").arg(fi.fileName()));
}

void SubtitleSearchDialog::downloadSubtitle(const SubtitleSearchResult &sub, std::function<void(const QString &filePath, bool ok)> onComplete) {
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/subtitles");
    QDir().mkpath(cacheDir);

    QString ext = sub.format.isEmpty() ? QStringLiteral("srt") : sub.format;
    QString safeFileName = sub.file;
    if (!safeFileName.endsWith(QLatin1Char('.') + ext, Qt::CaseInsensitive)) {
        safeFileName += QLatin1Char('.') + ext;
    }
    safeFileName.replace(QLatin1Char('/'), QLatin1Char('_'));
    QString targetPath = cacheDir + QLatin1Char('/') + safeFileName;

    // Check if already in cache
    if (QFile::exists(targetPath) && QFile(targetPath).size() > 0) {
        if (onComplete) onComplete(targetPath, true);
        return;
    }

    if (sub.downloadUrl.isEmpty()) {
        if (onComplete) onComplete(QString(), false);
        return;
    }

    m_statusLabel->setText(QStringLiteral("Downloading %1...").arg(sub.file));
    m_loadSelectedBtn->setEnabled(false);

    QNetworkRequest request(QUrl(sub.downloadUrl));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("VLSub 0.10.2"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_netManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, targetPath, onComplete]() {
        m_loadSelectedBtn->setEnabled(true);
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText(QStringLiteral("Download failed: %1").arg(reply->errorString()));
            if (onComplete) onComplete(QString(), false);
            return;
        }

        QByteArray rawData = reply->readAll();
        if (rawData.isEmpty()) {
            m_statusLabel->setText(QStringLiteral("Received empty subtitle file from server."));
            if (onComplete) onComplete(QString(), false);
            return;
        }

        QByteArray decompressed = decompressGzip(rawData);

        QFile outFile(targetPath);
        if (!outFile.open(QIODevice::WriteOnly)) {
            m_statusLabel->setText(QStringLiteral("Failed to write subtitle file to disk."));
            if (onComplete) onComplete(QString(), false);
            return;
        }

        outFile.write(decompressed);
        outFile.close();

        m_statusLabel->setText(QStringLiteral("Subtitle downloaded successfully."));
        if (onComplete) onComplete(targetPath, true);
    });
}

void SubtitleSearchDialog::loadSelectedSubtitle() {
    int row = m_resultsTable->currentRow();
    if (row < 0 || row >= m_currentResults.size()) {
        m_statusLabel->setText(QStringLiteral("Please select a subtitle row first."));
        return;
    }

    const auto &sub = m_currentResults[row];
    if (sub.provider == QStringLiteral("Local File") || sub.downloadUrl.isEmpty()) {
        loadSubtitleIntoMpv(sub.file, true);
        m_statusLabel->setText(QStringLiteral("Loaded subtitle: %1").arg(QFileInfo(sub.file).fileName()));
        return;
    }

    downloadSubtitle(sub, [this, sub](const QString &filePath, bool ok) {
        if (ok && !filePath.isEmpty()) {
            loadSubtitleIntoMpv(filePath, true);
            m_statusLabel->setText(QStringLiteral("Successfully loaded subtitle: %1").arg(sub.file));
        }
    });
}

void SubtitleSearchDialog::saveSelectedSubtitleAs() {
    int row = m_resultsTable->currentRow();
    if (row < 0 || row >= m_currentResults.size()) {
        m_statusLabel->setText(QStringLiteral("Please select a subtitle row first."));
        return;
    }

    const auto &sub = m_currentResults[row];
    downloadSubtitle(sub, [this, sub](const QString &filePath, bool ok) {
        if (!ok || filePath.isEmpty()) return;

        QString savePath = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("Save Subtitle As..."),
            QDir::homePath() + QStringLiteral("/") + QFileInfo(filePath).fileName(),
            QStringLiteral("Subtitles (*.srt *.ass *.ssa *.vtt);;All Files (*)")
        );

        if (!savePath.isEmpty()) {
            QFile::remove(savePath);
            if (QFile::copy(filePath, savePath)) {
                m_statusLabel->setText(QStringLiteral("Subtitle saved to: %1").arg(savePath));
            } else {
                m_statusLabel->setText(QStringLiteral("Failed to save subtitle."));
            }
        }
    });
}

void SubtitleSearchDialog::openSubtitleWebpage() {
    int row = m_resultsTable->currentRow();
    if (row < 0 || row >= m_currentResults.size()) {
        m_statusLabel->setText(QStringLiteral("Please select a subtitle row first."));
        return;
    }

    const auto &sub = m_currentResults[row];
    if (!sub.webUrl.isEmpty()) {
        QDesktopServices::openUrl(QUrl(sub.webUrl));
    } else {
        m_statusLabel->setText(QStringLiteral("No webpage available for this item."));
    }
}

void SubtitleSearchDialog::addCheckedSubtitles() {
    int count = 0;
    for (int i = 0; i < m_resultsTable->rowCount(); ++i) {
        auto *item = m_resultsTable->item(i, 0);
        if (item && item->checkState() == Qt::Checked && i < m_currentResults.size()) {
            const auto &sub = m_currentResults[i];
            bool isFirst = (count == 0);
            downloadSubtitle(sub, [this, isFirst](const QString &filePath, bool ok) {
                if (ok && !filePath.isEmpty()) {
                    loadSubtitleIntoMpv(filePath, isFirst);
                }
            });
            count++;
        }
    }

    if (count == 0) {
        m_statusLabel->setText(QStringLiteral("No items are checked in the table."));
    } else {
        m_statusLabel->setText(QStringLiteral("Adding %1 checked subtitles...").arg(count));
    }
}

void SubtitleSearchDialog::saveCheckedSubtitles() {
    QString targetDir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Select Directory to Save Subtitles"),
        QDir::homePath()
    );
    if (targetDir.isEmpty()) return;

    int count = 0;
    for (int i = 0; i < m_resultsTable->rowCount(); ++i) {
        auto *item = m_resultsTable->item(i, 0);
        if (item && item->checkState() == Qt::Checked && i < m_currentResults.size()) {
            const auto &sub = m_currentResults[i];
            downloadSubtitle(sub, [targetDir](const QString &filePath, bool ok) {
                if (ok && !filePath.isEmpty()) {
                    QString dest = targetDir + QStringLiteral("/") + QFileInfo(filePath).fileName();
                    QFile::remove(dest);
                    QFile::copy(filePath, dest);
                }
            });
            count++;
        }
    }

    if (count == 0) {
        m_statusLabel->setText(QStringLiteral("No items checked to save."));
    } else {
        m_statusLabel->setText(QStringLiteral("Saving %1 subtitles to %2...").arg(count).arg(targetDir));
    }
}

void SubtitleSearchDialog::resendRequest() {
    searchByTitle();
}

void SubtitleSearchDialog::openSettings() {
    QMessageBox::information(
        this,
        QStringLiteral("Subtitle Downloader Settings"),
        QStringLiteral("Primary Provider: OpenSubtitles REST API (active)\n"
                       "User Agent: VLSub 0.10.2\n"
                       "Format Support: SRT, ASS, SSA, VTT\n"
                       "Decompression: Native ZLIB GZIP inflate\n\n"
                       "All search results and downloads are fetched live from OpenSubtitles.")
    );
}

void SubtitleSearchDialog::loadSubtitleIntoMpv(const QString &subtitlePath, bool select) {
    if (!m_mpv) return;

    QByteArray pathBytes = subtitlePath.toUtf8();
    const char *action = select ? "select" : "auto";
    const char *cmd[] = {"sub-add", pathBytes.constData(), action, nullptr};
    int err = mpv_command_async(m_mpv, 0, cmd);
    if (err >= 0) {
        qDebug() << "[SubtitleSearchDialog] Injected subtitle into mpv:" << subtitlePath;
    } else {
        qWarning() << "[SubtitleSearchDialog] Failed to inject subtitle into mpv:" << mpv_error_string(err);
    }
}

} // namespace PotPlayer
