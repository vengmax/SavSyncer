#pragma once

#include "defines.h"

#include <QtWidgets/QMainWindow>
#include "ui_SavSyncer.h"

#include <QMouseEvent>
#include <QMenu>
#include <QFileDialog>
#include <QFileInfo>
#include <QTextEdit>
#include <QJsonDocument>
#include <QThread>
#include <QGraphicsOpacityEffect>
#include <QJsonArray>
#include <QSystemTrayIcon>
#include <QDialogButtonBox>

#include "About.h"
#include "Profile.h"
#include "Settings.h"
#include "Game.h"

class SavSyncer : public QMainWindow
{
    Q_OBJECT

public:
    SavSyncer(QWidget *parent = nullptr);
    ~SavSyncer();

public slots:

    void onAboutToQuit();

private slots:

    // find a game from the list
    void searchGameList(const QString& text);

    // profile event
    void handlerSuccessfulSignIn();
    void handlerFailedSignIn();
    void handlerSignOut();

    // new game
    Game* addNewGame();
    void clickedAddGame();

    // sync and del game
    QByteArray serializeFolder(const QString& folderPath);
    bool setLastModifiedTime(const QString& filePath, const QDateTime& newTime);
    void deserializeFolder(const QByteArray& jsonData, const QString& targetPath, bool &ok);
    QStringList findFiles(const QString& path, const QString& fileName);
    bool removeDirectory(const QString& dirPath);
    void syncGame(Game* game);
    void syncAllGame();
    void deleteGame(Game* game);

    // select game
    void selectGame(Game* game);

    // sync and del game from the queue
    void syncNextGame();
    void deleteNextGame();

    // backup
    void backup();
    QByteArray loadBackup();
    void removeBackup();

signals:

    // signals
    void startSyncGame(Game *game);
    void syncWaitingGame();
    void deleteWaitingGame();

protected:

    // events
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* object, QEvent* event);
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, long* result) override;

private:
    Ui::SavSyncer ui;
    QSettings* regSettings = nullptr;
    QString version = "0.2.1 (alpha)";
    QString buildDate = QDateTime::currentDateTime().toString("dd.MM.yyyy");
    QString versionAPI = "0.2 (alpha)";
    QString contact = "maksimuchhka@list.ru";

    // widgets
    About* about = nullptr;
    Profile* profile = nullptr;
    Settings* settings = nullptr;

    // tray
    QSystemTrayIcon* trayIcon = nullptr;

    QAction* actionAuth = nullptr;

    // vars game
    Game* selectedGame = nullptr;
    QList<Game*> listGame;
    QList<Game*> listSyncGame;
    QList<Game*> listDeleteGame;

    // .ss
    QJsonDocument docServiceFile;
    int maxIdSave = -1;

    // error message
    QMessageBox *msgSignOut = nullptr;

    // dragging
    bool dragging = false;
    QPoint dragStartPosition;

    // resizing 
    int indentResize = 5;
    bool resizingWidthLeft = false;
    bool resizingWidthRigth = false;
    bool resizingHeightTop = false;
    bool resizingHeightBottom = false;
    QPoint lastMousePosition;
    QSize lastSize;

    // styles
    QString styleSheetLineEditSearchGame;
    QString styleSheetBtnClearSearchGame;
    QString styleSheetBtnSearchGame;

    // close app
    bool forceClose = false;
};
