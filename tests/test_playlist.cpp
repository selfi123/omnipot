#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include "playlist/dpl_parser.h"
#include "playlist/m3u_parser.h"
#include "playlist/playlist_manager.h"
#include "config/settings_manager.h"

class TestPlaylist : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void testDplParserRoundTrip() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QString dplPath = tempDir.filePath("test.dpl");

        QList<PotPlayer::PlaylistItem> originalItems;
        PotPlayer::PlaylistItem item1;
        item1.filePath = "/videos/movie1.mp4";
        item1.title = "Movie 1 (Special Edition)";
        item1.duration = 7200.5; // 7,200,500 ms
        item1.startOffset = 120.0;
        originalItems.append(item1);

        PotPlayer::PlaylistItem item2;
        item2.filePath = "/videos/movie2.mkv";
        item2.title = "movie2.mkv";
        item2.duration = 3600.0;
        item2.startOffset = 0.0;
        originalItems.append(item2);

        bool saved = PotPlayer::DplParser::save(dplPath, originalItems, item1.filePath, 350.0);
        QVERIFY(saved);
        QVERIFY(QFile::exists(dplPath));

        QString curPlaying;
        double playtime = 0.0;
        QList<PotPlayer::PlaylistItem> loadedItems = PotPlayer::DplParser::load(dplPath, &curPlaying, &playtime);

        QCOMPARE(loadedItems.size(), 2);
        QCOMPARE(curPlaying, QString("/videos/movie1.mp4"));
        QCOMPARE(playtime, 350.0);

        QCOMPARE(loadedItems[0].filePath, QString("/videos/movie1.mp4"));
        QCOMPARE(loadedItems[0].title, QString("Movie 1 (Special Edition)"));
        QCOMPARE(static_cast<int>(loadedItems[0].duration), 7200);
        QCOMPARE(static_cast<int>(loadedItems[0].startOffset), 120);

        QCOMPARE(loadedItems[1].filePath, QString("/videos/movie2.mkv"));
        QCOMPARE(loadedItems[1].title, QString("movie2.mkv"));
        QCOMPARE(static_cast<int>(loadedItems[1].duration), 3600);
    }

    void testM3uParserRoundTrip() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QString m3uPath = tempDir.filePath("test.m3u8");

        QList<PotPlayer::PlaylistItem> originalItems;
        PotPlayer::PlaylistItem item1;
        item1.filePath = "/music/song1.flac";
        item1.title = "Artist - Song 1";
        item1.duration = 245.0;
        originalItems.append(item1);

        PotPlayer::PlaylistItem item2;
        item2.filePath = "/music/song2.mp3";
        item2.title = "song2.mp3";
        item2.duration = 180.0;
        originalItems.append(item2);

        bool saved = PotPlayer::M3uParser::save(m3uPath, originalItems);
        QVERIFY(saved);

        QList<PotPlayer::PlaylistItem> loadedItems = PotPlayer::M3uParser::load(m3uPath);
        QCOMPARE(loadedItems.size(), 2);
        QCOMPARE(loadedItems[0].filePath, QString("/music/song1.flac"));
        QCOMPARE(loadedItems[0].title, QString("Artist - Song 1"));
        QCOMPARE(static_cast<int>(loadedItems[0].duration), 245);

        QCOMPARE(loadedItems[1].filePath, QString("/music/song2.mp3"));
        QCOMPARE(loadedItems[1].title, QString("song2.mp3"));
    }

    void testPlaylistManagerOperations() {
        PotPlayer::PlaylistManager manager;
        QSignalSpy changeSpy(&manager, &PotPlayer::PlaylistManager::playlistChanged);
        QSignalSpy itemSpy(&manager, &PotPlayer::PlaylistManager::currentItemChanged);

        QCOMPARE(manager.count(), 0);
        QCOMPARE(manager.currentIndex(), -1);

        PotPlayer::PlaylistItem it1{"/path/1.mp4", "Track 1", 100.0, 0.0};
        PotPlayer::PlaylistItem it2{"/path/2.mp4", "Track 2", 200.0, 0.0};
        PotPlayer::PlaylistItem it3{"/path/3.mp4", "Track 3", 300.0, 0.0};

        manager.addItem(it1);
        manager.addItem(it2);
        manager.addItem(it3);

        QCOMPARE(manager.count(), 3);
        QCOMPARE(changeSpy.count(), 3);

        // Test Model Data
        QModelIndex idx0 = manager.index(0, 0);
        QVERIFY(idx0.isValid());
        QCOMPARE(manager.data(idx0, PotPlayer::PlaylistManager::FilePathRole).toString(), QString("/path/1.mp4"));
        QCOMPARE(manager.data(idx0, PotPlayer::PlaylistManager::TitleRole).toString(), QString("Track 1"));

        // Selection & Current Item
        manager.setCurrentIndex(0);
        QCOMPARE(manager.currentIndex(), 0);
        QCOMPARE(itemSpy.count(), 1);
        QCOMPARE(manager.currentItem().filePath, QString("/path/1.mp4"));

        // Next / Previous Navigation (Default RepeatMode::None)
        QVERIFY(manager.hasNext());
        QVERIFY(!manager.hasPrevious());

        manager.next();
        QCOMPARE(manager.currentIndex(), 1);
        QVERIFY(manager.hasNext());
        QVERIFY(manager.hasPrevious());

        manager.next();
        QCOMPARE(manager.currentIndex(), 2);
        QVERIFY(!manager.hasNext());

        // RepeatMode::All
        manager.setRepeatMode(PotPlayer::RepeatMode::All);
        QVERIFY(manager.hasNext());
        manager.next();
        QCOMPARE(manager.currentIndex(), 0); // Wraps around

        // RepeatMode::Track
        manager.setRepeatMode(PotPlayer::RepeatMode::Track);
        int beforeSpyCount = itemSpy.count();
        manager.next();
        QCOMPARE(manager.currentIndex(), 0); // Stays on track 0
        QCOMPARE(itemSpy.count(), beforeSpyCount + 1);

        // Move Item
        manager.setRepeatMode(PotPlayer::RepeatMode::None);
        manager.moveItem(0, 2); // move track 1 to end
        QCOMPARE(manager.data(manager.index(2, 0), PotPlayer::PlaylistManager::TitleRole).toString(), QString("Track 1"));
        QCOMPARE(manager.currentIndex(), 2);

        // Remove Item
        manager.removeItem(2);
        QCOMPARE(manager.count(), 2);

        // Clear
        manager.clear();
        QCOMPARE(manager.count(), 0);
        QCOMPARE(manager.currentIndex(), -1);
    }

    void testSettingsManager() {
        auto &settings = PotPlayer::SettingsManager::instance();

        // Volume & Mute
        settings.setVolume(85);
        QCOMPARE(settings.volume(), 85);
        settings.setMuted(true);
        QCOMPARE(settings.isMuted(), true);
        settings.setMuted(false);
        QCOMPARE(settings.isMuted(), false);

        // Speed & Hwdec
        settings.setSpeed(1.25);
        QCOMPARE(settings.speed(), 1.25);
        settings.setHwdec("vaapi");
        QCOMPARE(settings.hwdec(), QString("vaapi"));

        // Resume Position
        QString testFile = "/media/test_movie.mkv";
        settings.setResumePosition(testFile, 1450.5);
        QCOMPARE(settings.resumePosition(testFile), 1450.5);

        // Clear resume position (< 5 seconds removes)
        settings.setResumePosition(testFile, 2.0);
        QCOMPARE(settings.resumePosition(testFile), 0.0);

        // Recent Files
        settings.clearRecentFiles();
        QCOMPARE(settings.recentFiles().size(), 0);

        settings.addRecentFile("/media/vid1.mp4");
        settings.addRecentFile("/media/vid2.mp4");
        QCOMPARE(settings.recentFiles().size(), 2);
        QCOMPARE(settings.recentFiles().first(), QString("/media/vid2.mp4"));
    }

    void testAdvancedSettingsAndPresets() {
        auto &settings = PotPlayer::SettingsManager::instance();

        settings.setVideoScaler(QStringLiteral("lanczos"));
        QCOMPARE(settings.videoScaler(), QStringLiteral("lanczos"));

        settings.setAudioChannels(QStringLiteral("5.1"));
        QCOMPARE(settings.audioChannels(), QStringLiteral("5.1"));

        settings.setAudioNormalization(true);
        QCOMPARE(settings.isAudioNormalization(), true);

        // Export presets
        QString tmpIni = QDir::tempPath() + QStringLiteral("/test_potplayer_presets.ini");
        bool exported = settings.exportSettings(tmpIni);
        QVERIFY(exported);
        QVERIFY(QFile::exists(tmpIni));

        // Reset to defaults
        settings.resetToDefaults();
        QCOMPARE(settings.videoScaler(), QStringLiteral("spline36"));
        QCOMPARE(settings.audioChannels(), QStringLiteral("stereo"));
        QCOMPARE(settings.isAudioNormalization(), true);

        // Import back
        bool imported = settings.importSettings(tmpIni);
        QVERIFY(imported);
        QCOMPARE(settings.videoScaler(), QStringLiteral("lanczos"));
        QCOMPARE(settings.audioChannels(), QStringLiteral("5.1"));
        QCOMPARE(settings.isAudioNormalization(), true);

        QFile::remove(tmpIni);
    }
};

QTEST_MAIN(TestPlaylist)
#include "test_playlist.moc"
