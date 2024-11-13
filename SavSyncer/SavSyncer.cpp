#include "SavSyncer.h"

SavSyncer::SavSyncer(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    this->installEventFilter(this);

    connect(this, SIGNAL(startSyncGame(Game*)), this, SLOT(syncGame(Game*)), Qt::QueuedConnection);
    connect(this, SIGNAL(syncWaitingGame()), this, SLOT(syncNextGame()), Qt::QueuedConnection);
    connect(this, SIGNAL(deleteWaitingGame()), this, SLOT(deleteNextGame()), Qt::QueuedConnection);

    regSettings = new QSettings("SavSyncer", "SavSyncer");

    // widgets
    about = new About(regSettings);
    profile = new Profile(regSettings);
    settings = new Settings(regSettings);

    about->setVersion(version);
    about->setBuildDate(buildDate);
    about->setVersionAPI(versionAPI);
    about->setContact(contact);

    // tray
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/logo.png"));
    trayIcon->setToolTip("SavSyncer");
    QMenu* menuTrayIcon = new QMenu(this);
    menuTrayIcon->addAction("Показать", [this]() { showWindowLastState(this); });
    menuTrayIcon->addAction("Синхронизировать", [this]() { syncAllGame(); });
    menuTrayIcon->addAction("Выйти", [this]() { forceClose = true; close(); });
    trayIcon->setContextMenu(menuTrayIcon);
    connect(trayIcon, &QSystemTrayIcon::activated, this, [this, menuTrayIcon](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::ActivationReason::Trigger)
            showWindowLastState(this);
        });
    trayIcon->show();

    // menu logo
    QMenu* menuLogo = new QMenu(this);
    actionAuth = menuLogo->addAction("Войти", this, [this]() { showWindowLastState(profile); });
    menuLogo->addSeparator();
    menuLogo->addAction("Настройки", this, [this]() { showWindowLastState(settings); });
    menuLogo->addSeparator();
    menuLogo->addAction("Выход", [this]() { forceClose = true; close(); });
    connect(ui.btnLogo, &QPushButton::clicked, [this, menuLogo]() {
        menuLogo->exec(ui.btnLogo->mapToGlobal(QPoint(0, ui.btnLogo->height())));
        });

    // menu reference
    QMenu* menuReference = new QMenu(this);
    menuReference->addAction("Служба поддержки", this, [this]() { showWindowLastState(about); });
    menuReference->addSeparator();
    menuReference->addAction("О SavSyncer", this, [this]() { showWindowLastState(about); });
    connect(ui.btnReference, &QPushButton::clicked, [this, menuReference]() {
        menuReference->exec(ui.btnReference->mapToGlobal(QPoint(0, ui.btnReference->height())));
        });

    // standart buttons
    connect(ui.btnCloseProgram, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui.btnMinimizedProgram, SIGNAL(clicked()), this, SLOT(showMinimized()));
    connect(ui.btnMaximizedProgram, &QPushButton::clicked, this, [this]() { 
        if (isMaximized()) {
            showNormal();
            ui.btnMaximizedProgram->setIcon(QIcon(QPixmap(":/fullScreen.png")));
        }
        else {
            showMaximized();
            ui.btnMaximizedProgram->setIcon(QIcon(QPixmap(":/windowScreen.png")));
        }
        });

    // btn auth
    connect(ui.btnAuth, &QPushButton::clicked, this, [this]() { showWindowLastState(profile); });

    // btns searching game from list
    connect(ui.btnSearchGame, SIGNAL(clicked()), ui.lineEditSearchGame, SLOT(setFocus()));
    connect(ui.btnClearSearchGame, SIGNAL(clicked()), ui.lineEditSearchGame, SLOT(clear()));
    connect(ui.lineEditSearchGame, SIGNAL(textChanged(const QString&)), this, SLOT(searchGameList(const QString&)));

    // add new game
    connect(ui.btnAddGame, SIGNAL(clicked()), this, SLOT(clickedAddGame()));

    // profile events
    connect(profile, SIGNAL(signalSuccessfulSignIn()), this, SLOT(handlerSuccessfulSignIn()));
    connect(profile, SIGNAL(signalFailedSignIn()), this, SLOT(handlerFailedSignIn()));
    connect(profile, SIGNAL(signalSignOut()), this, SLOT(handlerSignOut()));

    // sync all
    connect(ui.btnSyncAll, SIGNAL(clicked()), this, SLOT(syncAllGame()));

    // style logo
    QGraphicsOpacityEffect* effectBtnMain = new QGraphicsOpacityEffect(ui.btnMain);
    effectBtnMain->setOpacity(0.3);
    ui.btnMain->setGraphicsEffect(effectBtnMain);
    QGraphicsOpacityEffect* effectLabelMain = new QGraphicsOpacityEffect(ui.labelMain);
    effectLabelMain->setOpacity(0.3);
    ui.labelMain->setGraphicsEffect(effectLabelMain);

    // auto sign in
    if (settings->getAutoSignIn())
        profile->signIn();
}

SavSyncer::~SavSyncer(){
    regSettings->deleteLater();

    about->deleteLater();
    profile->deleteLater();
    settings->deleteLater();

    if (msgSignOut)
        delete msgSignOut;
}

void SavSyncer::onAboutToQuit() {
    if (!listSyncGame.isEmpty() || !listDeleteGame.isEmpty())
        backup();
}

void SavSyncer::showWindowLastState(QWidget* widget) {
    if (widget) {
        widget->setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        widget->show();
        widget->raise();
    }
}

void SavSyncer::searchGameList(const QString& text) {
    for (auto game : listGame) {
        if (game->name().toUpper().contains(text.toUpper()))
            game->showItem();
        else
            game->hideItem();
    }
}

void SavSyncer::handlerSuccessfulSignIn() {
    docServiceFile = QJsonDocument();
    maxIdSave = -1;

    // back up before sign in if backup file is exists
    QByteArray backupData = loadBackup();
    if (!backupData.isEmpty() && !QJsonDocument::fromJson(QByteArray::fromBase64(backupData)).isNull()) {

        // show window
        showWindowLastState(this);

        QMessageBox::information(nullptr, "Резервное копирование", "Предыдущее сеанс работы программы был завершен некорректно. "
            "Программа вернется до момента последнего успешного действия! Полжалуйста проверте актуальность заданных параметров. ");
        profile->uploadGameData(".ss", backupData);
        if (profile->error()) {
            CRITICAL_MSG("Upload backup service file \".ss\" failed");

            // show window
            showWindowLastState(this);

            // message
            QMessageBox msg;
            msg.setIcon(QMessageBox::Critical);
            msg.setWindowTitle("Ошибка синхронизации");
            msg.setWindowFlags(msg.windowFlags() & ~Qt::AA_DisableWindowContextHelpButton);
            msg.setText("Не удалось корректно распознать игровые данные пользователя на удаленном диске. (Upload backup file \".ss\" failed)");
            msg.setStandardButtons(QMessageBox::Yes);
            msg.addButton(QMessageBox::No);
            msg.setDefaultButton(QMessageBox::Yes);
            QAbstractButton* yesButton = msg.button(QMessageBox::Yes);
            yesButton->setText("Повторить попытку");
            yesButton->setMinimumWidth(150);
            QAbstractButton* noButton = msg.button(QMessageBox::No);
            noButton->setText("Выйти");
            int res = msg.exec();
            if (res == QMessageBox::Yes)
                handlerSuccessfulSignIn();
            else
                profile->signOut();
            return;
        }
    }
    removeBackup();

    QApplication::setOverrideCursor(Qt::BusyCursor);

    // load service file .ss
    QJsonDocument documentJSON;
    bool fileExistence = profile->checkFileExistence("", ".ss");
    if (profile->error()) {
        CRITICAL_MSG("Failed to read user data correctly");

        // show window
        showWindowLastState(this);

        QMessageBox::critical(nullptr, "Ошибка авторизации", "Не удалось корректно прочиать данные о пользователе! Пожалуйста перезайдите в акаунт.");
        handlerSignOut();
        emit deleteWaitingGame();
        return;
    }
    if (fileExistence) {
        QByteArray jsonData = profile->downloadGameData(".ss");
        if (profile->error()) {
            CRITICAL_MSG("Download service file \".ss\" failed");
            QApplication::restoreOverrideCursor();

            // show window
            showWindowLastState(this);

            // message
            QMessageBox msg;
            msg.setIcon(QMessageBox::Critical);
            msg.setWindowTitle("Ошибка игровых данных");
            msg.setWindowFlags(msg.windowFlags() & ~Qt::AA_DisableWindowContextHelpButton);
            msg.setText("Не удалось корректно распознать игровые данные пользователя на удаленном диске. (Download file \".ss\" failed)");
            msg.setStandardButtons(QMessageBox::Yes); 
            msg.addButton(QMessageBox::No);
            msg.setDefaultButton(QMessageBox::Yes);
            QAbstractButton* yesButton = msg.button(QMessageBox::Yes);
            yesButton->setText("Повторить попытку");
            yesButton->setMinimumWidth(150);
            QAbstractButton* noButton = msg.button(QMessageBox::No);
            noButton->setText("Выйти");
            int res = msg.exec();
            if (res == QMessageBox::Yes)
                handlerSuccessfulSignIn();
            else
                profile->signOut();
            return;
        }
        if (!jsonData.isEmpty()) {
            jsonData = QByteArray::fromBase64(jsonData);
            documentJSON = QJsonDocument::fromJson(jsonData);
            if (documentJSON.isNull() || !documentJSON.isObject()) {
                CRITICAL_MSG("Data is not a JSON file");
                QApplication::restoreOverrideCursor();

                // show window
                showWindowLastState(this);

                // message
                QMessageBox msg;
                msg.setIcon(QMessageBox::Critical);
                msg.setWindowTitle("Ошибка игровых данных");
                msg.setWindowFlags(msg.windowFlags() & ~Qt::AA_DisableWindowContextHelpButton);
                msg.setText("Не удалось корректно распознать игровые данные пользователя на удаленном диске. (File \".ss\" is incorrect)");
                msg.setStandardButtons(QMessageBox::Yes);
                msg.addButton(QMessageBox::No);
                msg.setDefaultButton(QMessageBox::Yes);
                QAbstractButton* yesButton = msg.button(QMessageBox::Yes);
                yesButton->setText("Повторить попытку");
                yesButton->setMinimumWidth(150);
                QAbstractButton* noButton = msg.button(QMessageBox::No);
                noButton->setText("Выйти");
                int res = msg.exec();
                if (res == QMessageBox::Yes)
                    handlerSuccessfulSignIn();
                else
                    profile->signOut();
                return;
            }

            // save .ss
            docServiceFile = QJsonDocument(documentJSON);

            QJsonObject objectJSON = documentJSON.object();
            for (QString& key : objectJSON.keys()) {

                // params for new game
                QJsonObject jsonGameInfo = objectJSON.value(key).toObject();
                QString pathGame = jsonGameInfo["path_game"].toString();
                QString pathGameSave = jsonGameInfo["path_gamesave"].toString();
                int gameId = key.toInt();
                if (gameId > maxIdSave)
                    maxIdSave = gameId;

                // info
                QFileInfo info(pathGame);

                // create game
                Game* game = addNewGame();
                game->setId(gameId);
                game->setFilePath(pathGame);
                game->setPathSave(pathGameSave);
                if (!info.exists()) {
                    game->setName(jsonGameInfo["name"].toString());
                    game->setVersion("-");
                    if (jsonGameInfo.contains("icon")) {
                        QByteArray dataIcon = QByteArray::fromBase64(jsonGameInfo["icon"].toString().toUtf8());
                        QPixmap pixmap;
                        pixmap.loadFromData(dataIcon, "PNG");
                        game->setIcon(QIcon(pixmap));
                    }
                    game->setSyncMode(SyncMode::SyncFailed);
                }
                QVariant variantSync = regSettings->value("Services/" + profile->getId() + "/" + game->name() + "/SyncEnabled");
                if (variantSync.isValid())
                    game->setSyncEnable(variantSync.toBool());
            }

            // sync all game
            if (settings->getAutoSync())
                syncAllGame();
        }
    }

    // update ui
    ui.btnAuth->setIcon(profile->getAvatar());
    ui.btnAuth->setText(" " + profile->getLastName() + " " + profile->getFirstName());
    actionAuth->setText("Профиль");
    QApplication::restoreOverrideCursor();
}

void SavSyncer::handlerFailedSignIn() {
    docServiceFile = QJsonDocument();
    maxIdSave = -1;

    // update ui
    ui.btnAuth->setIcon(QIcon());
    ui.btnAuth->setText("Войти");
    actionAuth->setText("Войти");
}

void SavSyncer::handlerSignOut() {

    if (msgSignOut)
        delete msgSignOut;

    for (auto game : listGame) {
        if (game->isBusy()) {
            msgSignOut = new QMessageBox(QMessageBox::Critical, "Ошибка", "В данный момент идет синхронизация, пожалуйста подождите.");
            msgSignOut->show();
            return;
        }
    }

    profile->signOut();

    docServiceFile = QJsonDocument();
    maxIdSave = -1;

    // update ui
    ui.btnAuth->setIcon(QIcon());
    ui.btnAuth->setText("Войти");
    actionAuth->setText("Войти");

    // clear list game
    selectGame(nullptr);
    qDeleteAll(listGame);
    listGame.clear();
}

Game* SavSyncer::addNewGame() {

    // Create item and page game
    ItemGameList* item = new ItemGameList(regSettings, ui.frameGameList);
    QVBoxLayout* layout = dynamic_cast<QVBoxLayout*>(ui.frameGameList->layout());
    layout->insertWidget(layout->count() - 1, item);
    PageGameList* page = new PageGameList(regSettings, ui.stackedWidgetGame);
    ui.stackedWidgetGame->addWidget(page);

    // Create game
    Game* game = new Game(item, page, regSettings);
    listGame.append(game);

    // setup game
    connect(game, &Game::enabledSync, this, [this, game](bool value) {
        regSettings->setValue("Services/" + profile->getId() + "/" + game->name() + "/SyncEnabled", value);
        });
    connect(game, &Game::closedGame, this, [this, game]() {
        INFO_MSG(game->name() + "is closed");
        if (!game->isBusy() && game->isSync() && settings->getAutoSync()) {
            game->setSyncMode(SyncMode::SyncProcess);
            emit startSyncGame(game);
        }
        });
    game->startMonitoringApp();

    connect(game, &Game::clickedItemMouseLeftButton, this, [this, game]() { selectGame(game); });

    QMenu* menuItemGameList = new QMenu(item);
    menuItemGameList->addAction("Синхронизировать", this, [this, game]() { 
        if (!game->isBusy() && game->isSync()) {
            game->setSyncMode(SyncMode::SyncProcess);
            emit startSyncGame(game);
        }
        });
    menuItemGameList->addSeparator();
    menuItemGameList->addAction("Настройки", this, [this, game]() { selectGame(game); });
    menuItemGameList->addSeparator();
    menuItemGameList->addAction("Удалить", this, [this, game]() { 

        if (!docServiceFile.isNull()) {
            if (docServiceFile.isObject()) {
                QJsonObject obj = docServiceFile.object();
                for (QString& key : obj.keys()) {
                    if (key.toInt() == game->id()) {
                        QMessageBox msg(QMessageBox::Question, "Удаление", "Все данные связанные с этой игрой на удаленном хранилище будут удалены. "
                            "Вы действительно хотите удалить игру?", QMessageBox::Yes);
                        msg.addButton(QMessageBox::Cancel);
                        msg.setDefaultButton(QMessageBox::Cancel);
                        msg.button(QMessageBox::Yes)->setText("Да");
                        msg.button(QMessageBox::Cancel)->setText("Отмена");
                        int res = msg.exec();
                        if (res != QMessageBox::Yes) {
                            game->setBusy(false);
                            emit deleteWaitingGame();
                            return;
                        }
                        break;
                    }
                }
            }
        }
        deleteGame(game);
        });

    connect(game, &Game::clickedItemMouseRightButton, [this, game, menuItemGameList]() {
        selectGame(game);
        if(!game->isBusy())
            menuItemGameList->exec(cursor().pos());
        });

    return game;
}

void SavSyncer::clickedAddGame() {

    if (!profile->isOnline()) {
        WARNING_MSG("User is not authorized");
        QMessageBox::warning(nullptr, "Предупреждение", "Необходимо войти в учетную запись.");
        return;
    }

    INFO_MSG("Start adding a new game");
    QString pathGame = QFileDialog::getOpenFileName(nullptr, "Добавить игру", "", "Executable file (*.exe)");
    if (pathGame.isEmpty()) {
        INFO_MSG("Adding canceled");
        return;
    }

    for (auto game : listGame) {
        if (game->filePath() == pathGame) {
            CRITICAL_MSG("This game already exists");
            QMessageBox::critical(nullptr, "Ошибка", "Такая игра уже есть!");
            return;
        }
    }

    // info
    QFileInfo info(pathGame);

    // create game
    Game* game = addNewGame();
    game->setId(++maxIdSave);
    game->setFilePath(pathGame);
    QVariant variantSync = regSettings->value("Services/" + profile->getId() + "/" + game->name() + "/SyncEnabled");
    if (variantSync.isValid())
        game->setSyncEnable(variantSync.toBool());

    INFO_MSG("New game added");
}

QByteArray SavSyncer::serializeFolder(const QString& folderPath, qint64& startSize, bool& ok) {
    ok = true;

    QJsonObject folderObject;
    QDir dir(folderPath);
    foreach(const QString & fileName, dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QString filePath = dir.absoluteFilePath(fileName);
        if (QFileInfo(filePath).isFile()) {
            QJsonObject fileObject;
            
            // check size file
            qint64 sizeFileBase64 = ceil(QFileInfo(filePath).size() / (double)3) * 4;
            if ((startSize + sizeFileBase64) >= (qint64)(1.4 * 1024 * 1024 * 1024 - 1024)) {
                CRITICAL_MSG("Maximum game data size exceeded (max size: 1 GB)");
                ok = false;
                return QByteArray();
            }

            // load file
            QFile file(filePath);
            QByteArray fileData;
            if (file.open(QIODevice::ReadOnly)) {
                fileData = file.readAll();
                file.close();
            }
            else {
                CRITICAL_MSG("Failed to open file " + filePath);
                ok = false;
                return QByteArray();
            }

            // create json
            fileObject["size"] = QFileInfo(filePath).size();
            fileObject["modified"] = QFileInfo(filePath).lastModified().toString("yyyy_MM_dd_hh-mm-ss-zzz");
            fileObject["data"] = ".";

            // check size file + json
            qint64 sizeJsonFile = startSize + QJsonDocument(fileObject).toJson(QJsonDocument::Compact).size() + sizeFileBase64;
            if (sizeJsonFile >= (qint64)(1.4 * 1024 * 1024 * 1024 - 1024)) {
                qDebug() << "Error: Maximum game data size exceeded (max size: 1 GB)";
                ok = false;
                return QByteArray();
            }

            fileObject["data"] = QString(fileData.toBase64());
            folderObject["file:" + fileName] = fileObject;

            int serviceSymbols = 6;
            startSize += QJsonDocument(fileObject).toJson(QJsonDocument::Compact).size() + QString("file:" + fileName).size() + serviceSymbols;
        }
        else {
            QByteArray dataJSON = serializeFolder(filePath, startSize, ok);
            if (!ok)
                return QByteArray();

            QJsonDocument subfolderObject = QJsonDocument::fromJson(dataJSON);
            folderObject["folder:" + fileName] = subfolderObject.object();

            int serviceSymbols = 6;
            startSize += QString("folder:" + fileName).size() + serviceSymbols;
        }
    }
    QJsonDocument document(folderObject);
    return document.toJson(QJsonDocument::Compact);
}

bool SavSyncer::setLastModifiedTime(const QString& filePath, const QDateTime& newTime) {
    HANDLE hFile = CreateFileA(
        filePath.toStdString().c_str(),
        GENERIC_WRITE,
        FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        return false;
    }

    ULARGE_INTEGER ull;
    ull.QuadPart = newTime.toMSecsSinceEpoch() * 10000 + 116444736000000000;

    FILETIME ft;
    ft.dwLowDateTime = ull.LowPart;
    ft.dwHighDateTime = ull.HighPart;

    // Установка времени последнего изменения
    if (!SetFileTime(hFile, NULL, NULL, &ft)) {
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);
    return true;
}

void SavSyncer::deserializeFolder(const QByteArray& jsonData, const QString& targetPath, bool& ok) {
    ok = true;

    QJsonDocument documentJSON = QJsonDocument::fromJson(jsonData);
    QJsonObject objectJSON = documentJSON.object();
    if (documentJSON.isNull() || !documentJSON.isObject()) {
        CRITICAL_MSG("Data is not a JSON file");
        ok = false;
        return;
    }

    QDir targetDir;
    targetDir.mkpath(targetPath);
    if (!targetDir.exists()) {
        CRITICAL_MSG("Failed to create folder");
        ok = false;
        return;
    }

    for (QString &key: objectJSON.keys()){
        if (key.startsWith("folder:")) {
            QByteArray data = QJsonDocument(objectJSON.value(key).toObject()).toJson(QJsonDocument::Compact);
            deserializeFolder(data, targetPath + "/" + key.remove("folder:"), ok);
            if (!ok)
                return;
        }
        else if (key.startsWith("file:")) {
            QJsonObject fileObject = objectJSON.value(key).toObject();
            if (fileObject.contains("data") && fileObject.contains("size") && fileObject.contains("modified")) {

                QByteArray base64Data = fileObject["data"].toString().toUtf8();
                QDateTime dateModified = QDateTime::fromString(fileObject["modified"].toString(), "yyyy_MM_dd_hh-mm-ss-zzz");
                QByteArray data = QByteArray::fromBase64(base64Data);

                QFile file(targetPath + "/" + key.remove("file:"));
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(data);
                    file.close();
                    setLastModifiedTime(file.fileName(), dateModified);
                }
                else {
                    CRITICAL_MSG("Cannot open file for writing");
                    ok = false;
                    return;
                }
            }
            else {
                CRITICAL_MSG("Data is not a correct JSON file");
                ok = false;
                return;
            }
        }
        else {
            CRITICAL_MSG("Data is not a JSON file");
            ok = false;
            return;
        }
    }
}

QStringList SavSyncer::findFiles(const QString& path, const QString& fileName) {
    
    QStringList output;

    QDir dir(path);
    if (!dir.exists())
        return QStringList();

    QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo& entry : entries) {
        if (entry.isDir())
            output.append(findFiles(entry.filePath(), fileName));
        else if (entry.fileName() == fileName)
            output.append(entry.filePath());
    }
    return output;
}

bool SavSyncer::removeDirectory(const QString& dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) {
        CRITICAL_MSG("Directory does not exist: " + dirPath);
        return false;
    }

    foreach(QString file, dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
        QString fullPath = dir.filePath(file);
        if (QFileInfo(fullPath).isDir())
            removeDirectory(fullPath);
        else
            QFile::remove(fullPath);
    }

    return dir.rmdir(dirPath);
}

void SavSyncer::syncGame(Game* game) {

    if (!profile->isOnline()) {
        WARNING_MSG("User is not authorized");

        // show window
        showWindowLastState(this);

        QMessageBox::warning(nullptr, "Предупреждение", "Необходимо войти в учетную запись.");
        return;
    }

    if (game->isBusy()) {
        CRITICAL_MSG("Unable to sync " + game->name() + "! The game is currently busy");
        return;
    }

    INFO_MSG("Start synchronization: " + game->name());

    if (!game->isSync()) {
        INFO_MSG("Synchronization for " + game->name() + " is disabled");
        game->setSyncMode(SyncMode::SyncDisabled);
        return;
    }

    if (!game->isValid()) {
        INFO_MSG("Not all data is filled in for " + game->name());
        game->setSyncMode(SyncMode::SyncFailed);
        return;
    }

    game->refreshInfo();
    if (game->saveSize() < (qint64)(1024 * 1024 * 1024)) {

        // if another game is syncing, then put in queue
        if (!listSyncGame.isEmpty()) {
            if (listSyncGame.first() != game) {
                if (!listSyncGame.contains(game))
                    listSyncGame.append(game);
                return;
            }
        }
        else
            listSyncGame.append(game);

        game->setBusy(true);
        game->setSyncMode(SyncMode::SyncProcess);

        // new thread
        QThread* anotherThread = new QThread();
        QObject* objectAnotherThread = new QObject();
        objectAnotherThread->moveToThread(anotherThread);
        QObject::connect(anotherThread, &QThread::finished, anotherThread, &QThread::deleteLater);
        QObject::connect(anotherThread, &QThread::finished, objectAnotherThread, &QThread::deleteLater);
        anotherThread->start();

        // local modified
        game->refreshInfo();
        QDateTime lastLocalGameDataModified = game->dateLastModified();
        QDateTime lastServiceGameDataModified = QDateTime(QDate(1, 1, 1), QTime(1, 1));

        // load and check param from .ss
        bool uploadSave = false;
        bool downloadSave = false;
        QString nameGameSave = "GameData_" + QString::number(game->id()) + ".savsyncer";
        QJsonDocument documentJSON(docServiceFile);
        QJsonObject objectJSON = documentJSON.object();
        for (QString& key : objectJSON.keys()) {
            if (key.toInt() == game->id()) {
                QJsonObject jsonGameInfo = objectJSON.value(key).toObject();
                lastServiceGameDataModified = QDateTime::fromString(jsonGameInfo["modified"].toString(), "yyyy_MM_dd_hh-mm-ss-zzz");
                nameGameSave = jsonGameInfo["name_gamesave"].toString();

                // question before sync
                QMessageBox msg;
                msg.setIcon(QMessageBox::Information);
                msg.setWindowTitle("Синхронизация данных");
                msg.setWindowFlags(msg.windowFlags() & ~Qt::AA_DisableWindowContextHelpButton);
                msg.setStandardButtons(QMessageBox::Yes);
                msg.addButton(QMessageBox::No);
                msg.addButton(QMessageBox::Cancel);
                msg.setDefaultButton(QMessageBox::Cancel);
                QAbstractButton* yesButton = msg.button(QMessageBox::Yes);
                QAbstractButton* noButton = msg.button(QMessageBox::No);
                noButton->setMinimumWidth(120);
                QAbstractButton* cancelButton = msg.button(QMessageBox::Cancel);
                cancelButton->setText("Отмена");

                // find data another user
                bool foundAnotherUser = false;
                QStringList filesSavSyncer = findFiles(game->pathGameSave(), ".savsyncer");
                for (QString itemFile : filesSavSyncer) {
                    QFile file(itemFile);
                    if (file.open(QIODevice::ReadOnly)) {
                        QString fileData = file.readAll();
                        file.close();
                        QString fileUserId = fileData.mid(fileData.indexOf("user_id") + QString("user_id: ").size(), fileData.indexOf("\r\n", fileData.indexOf("user_id")) 
                            - (fileData.indexOf("user_id") + QString("user_id: ").size()));
                        if (fileUserId != profile->getId()) {

                            // show window
                            showWindowLastState(this);

                            msg.setText("Данные игры " + game->name() + " или часть данных принадлежат другому пользователю. "
                                "Хотите ли вы полностью заменить их на свои данные или сохранить эти данные к себе на диск?");
                            yesButton->setText("Заменить");
                            noButton->setText("Сохранить на диск");
                            int res = msg.exec();
                            if (res == QMessageBox::Yes) {
                                removeDirectory(game->pathGameSave());
                                downloadSave = true;
                            }
                            else if (res == QMessageBox::No) {
                                for (QString& rmFile : filesSavSyncer)
                                    QFile::remove(rmFile);
                                uploadSave = true;
                            }
                            else {
                                anotherThread->quit();
                                game->setBusy(false);
                                game->setSyncMode(SyncMode::SyncUncompleted);
                                emit syncWaitingGame();
                                return;
                            }

                            foundAnotherUser = true;
                            break;
                        }
                    }
                }
                if (foundAnotherUser)
                    break;
                else if (!filesSavSyncer.isEmpty()) {
                    if (jsonGameInfo["name"].toString() != game->name() || jsonGameInfo["path_game"].toString() != game->filePath() ||
                        jsonGameInfo["path_gamesave"].toString() != game->pathGameSave() || 
                        !QFileInfo(game->pathGameSave() + "/.savsyncer").exists())
                    {

                        // show window
                        showWindowLastState(this);

                        msg.setText("Были изменены данные игры " + game->name() + ". Для продолжения выберите где находятся актуальные данные.");
                        yesButton->setText("На диске");
                        noButton->setText("На компьютере");
                        int res = msg.exec();
                        if (res == QMessageBox::Yes)
                            downloadSave = true;
                        else if (res == QMessageBox::No)
                            uploadSave = true;
                        else {
                            anotherThread->quit();
                            game->setBusy(false);
                            game->setSyncMode(SyncMode::SyncUncompleted);
                            emit syncWaitingGame();
                            return;
                        }
                    }
                }
                else {

                    // show window
                    showWindowLastState(this);

                    msg.setText("Были изменены данные игры " + game->name() + ". Для продолжения выберите где находятся актуальные данные.");
                    yesButton->setText("На диске");
                    noButton->setText("На компьютере");
                    int res = msg.exec();
                    if (res == QMessageBox::Yes)
                        downloadSave = true;
                    else if (res == QMessageBox::No)
                        uploadSave = true;
                    else {
                        anotherThread->quit();
                        game->setBusy(false);
                        game->setSyncMode(SyncMode::SyncUncompleted);
                        emit syncWaitingGame();
                        return;
                    }
                }
                break;
            }
        }

        // sync
        QEventLoop eveloop;
        QByteArray data;
        QString path = game->pathGameSave();
        if (lastLocalGameDataModified != lastServiceGameDataModified || uploadSave || downloadSave) {
            if ((lastServiceGameDataModified < lastLocalGameDataModified || uploadSave) && !downloadSave) {

                // create local .savsyncer
                QFile file(game->pathGameSave() + "/.savsyncer");
                if (file.open(QIODevice::WriteOnly)) {
                    QString output = "---SavSyncer---\r\nVersion: " + version + "\r\nBuild date: "
                        + buildDate + "\r\nVersion API: " + versionAPI + "\r\nContact: " + contact
                        + "\r\n\r\n[" + profile->serviceName() + "]\r\nuser_id: " + profile->getId();
                    file.write(output.toUtf8());
                    file.close();
                    setLastModifiedTime(file.fileName(), lastLocalGameDataModified);
                }
                else {
                    anotherThread->quit();
                    game->setBusy(false);
                    game->setSyncMode(SyncMode::SyncFailed);
                    CRITICAL_MSG(game->name() + ": Create local file \".savsyncer\" failed");
                    if (!isVisible())
                        trayIcon->showMessage("Ошибка синхронизации", "Произошла ошибка при синхронизации " + game->name(), QSystemTrayIcon::Warning);
                    emit syncWaitingGame();
                    return;
                }

                // upload game save
                bool ok = true;
                qint64 startSize = 0;
                auto funcCompress = [this, path, &data, &eveloop, game, &startSize, &ok]() {

                    INFO_MSG("Start serialize data " + game->name());
                    data = serializeFolder(path, startSize, ok);
                    INFO_MSG("Finished serialize data " + game->name());

                    if (ok) {
                        INFO_MSG("Start compress data " + game->name());
                        data = qCompress(data, 4);
                        INFO_MSG("Finished compress data " + game->name());
                    }

                    eveloop.quit();
                    };
                if (QMetaObject::invokeMethod(objectAnotherThread, funcCompress, Qt::QueuedConnection))
                    eveloop.exec();

                if (!ok) {
                    anotherThread->quit();
                    game->setBusy(false);
                    game->setSyncMode(SyncMode::SyncFailed);
                    CRITICAL_MSG(game->name() + ": Serialization of game data failed");
                    if (!isVisible())
                        trayIcon->showMessage("Ошибка синхронизации", "Произошла ошибка при синхронизации " + game->name(), QSystemTrayIcon::Warning);
                    emit syncWaitingGame();
                    return;
                }

                profile->uploadGameData(nameGameSave, data);
                if (profile->error()) {
                    anotherThread->quit();
                    game->setBusy(false);
                    game->setSyncMode(SyncMode::SyncFailed);
                    CRITICAL_MSG("Synchronization " + game->name() + " failed");
                    if (!isVisible())
                        trayIcon->showMessage("Ошибка синхронизации", "Произошла ошибка при синхронизации " + game->name(), QSystemTrayIcon::Warning);
                    emit syncWaitingGame();
                    return;
                }

                // create .ss
                QJsonObject jsonObjectGameData;
                jsonObjectGameData["name"] = game->name();
                QByteArray dataIcon;
                QBuffer buffer(&dataIcon);
                buffer.open(QIODevice::WriteOnly);
                game->icon().pixmap(256, 256).save(&buffer, "PNG");
                jsonObjectGameData["icon"] = QString(dataIcon.toBase64());
                jsonObjectGameData["modified"] = lastLocalGameDataModified.toString("yyyy_MM_dd_hh-mm-ss-zzz");
                jsonObjectGameData["path_game"] = game->filePath();
                jsonObjectGameData["path_gamesave"] = game->pathGameSave();
                jsonObjectGameData["name_gamesave"] = nameGameSave;
                QJsonObject jsonObject;
                if (!documentJSON.isEmpty())
                    jsonObject = documentJSON.object();
                jsonObject[QString::number(game->id())] = jsonObjectGameData;
                documentJSON = QJsonDocument(jsonObject);

                // upload .ss
                QByteArray dataSS = documentJSON.toJson();
                dataSS = dataSS.toBase64();
                profile->uploadGameData(".ss", dataSS);
                if (profile->error()) {
                    anotherThread->quit();
                    game->setBusy(false);
                    game->setSyncMode(SyncMode::SyncFailed);
                    CRITICAL_MSG(game->name() + ": Upload service file \".ss\" failed");
                    if (!isVisible())
                        trayIcon->showMessage("Ошибка синхронизации", "Произошла ошибка при синхронизации " + game->name(), QSystemTrayIcon::Warning);
                    emit syncWaitingGame();
                    return;
                }

                // save new service file .ss
                docServiceFile = QJsonDocument(documentJSON);
            }
            else {

                // download game save
                data = profile->downloadGameData(nameGameSave);
                if (data.isEmpty()) {
                    anotherThread->quit();
                    game->setBusy(false);
                    game->setSyncMode(SyncMode::SyncFailed);
                    CRITICAL_MSG("Synchronization " + game->name() + " failed");
                    if (!isVisible())
                        trayIcon->showMessage("Ошибка синхронизации", "Произошла ошибка при синхронизации " + game->name(), QSystemTrayIcon::Warning);
                    emit syncWaitingGame();
                    return;
                }
                
                bool ok = true;
                auto funcUncompress = [this, path, &data, &eveloop, game, &ok]() {

                    INFO_MSG("Start uncompress data " + game->name());
                    data = qUncompress(data);
                    INFO_MSG("Finished uncompress data " + game->name());

                    INFO_MSG("Start deserialize data " + game->name());
                    deserializeFolder(data, path, ok);
                    INFO_MSG("Finished deserialize data " + game->name());

                    eveloop.quit();
                    };
                if (QMetaObject::invokeMethod(objectAnotherThread, funcUncompress, Qt::QueuedConnection))
                    eveloop.exec();

                if (!ok) {
                    anotherThread->quit();
                    game->setBusy(false);
                    game->setSyncMode(SyncMode::SyncFailed);
                    CRITICAL_MSG(game->name() + ": Deserialization of game data failed");
                    if (!isVisible())
                        trayIcon->showMessage("Ошибка синхронизации", "Произошла ошибка при синхронизации " + game->name(), QSystemTrayIcon::Warning);
                    emit syncWaitingGame();
                    return;
                }

                // update info game save
                game->refreshInfo();
                lastLocalGameDataModified = game->dateLastModified();

                // create .ss
                QJsonObject jsonObjectGameData;
                jsonObjectGameData["name"] = game->name();
                QByteArray dataIcon;
                QBuffer buffer(&dataIcon);
                buffer.open(QIODevice::WriteOnly);
                game->icon().pixmap(256, 256).save(&buffer, "PNG");
                jsonObjectGameData["icon"] = QString(dataIcon.toBase64());
                jsonObjectGameData["modified"] = lastLocalGameDataModified.toString("yyyy_MM_dd_hh-mm-ss-zzz");
                jsonObjectGameData["path_game"] = game->filePath();
                jsonObjectGameData["path_gamesave"] = game->pathGameSave();
                jsonObjectGameData["name_gamesave"] = nameGameSave;
                QJsonObject jsonObject;
                if (!documentJSON.isEmpty())
                    jsonObject = documentJSON.object();
                jsonObject[QString::number(game->id())] = jsonObjectGameData;
                documentJSON = QJsonDocument(jsonObject);

                // upload .ss
                QByteArray dataSS = documentJSON.toJson();
                dataSS = dataSS.toBase64();
                profile->uploadGameData(".ss", dataSS);
                if (profile->error()) {
                    anotherThread->quit();
                    game->setBusy(false);
                    game->setSyncMode(SyncMode::SyncFailed);
                    CRITICAL_MSG(game->name() + ": Upload service file \".ss\" failed");
                    if (!isVisible())
                        trayIcon->showMessage("Ошибка синхронизации", "Произошла ошибка при синхронизации " + game->name(), QSystemTrayIcon::Warning);
                    emit syncWaitingGame();
                    return;
                }

                // save new service file .ss
                docServiceFile = QJsonDocument(documentJSON);
            }
        }

        anotherThread->quit();
        INFO_MSG("Synchronization " + game->name() + " was completed successfully");
        game->setDateLastSync(QDateTime::currentDateTime());
        profile->loadUserData();
        game->setBusy(false);
        game->setSyncMode(SyncMode::SyncCompleted);
        emit syncWaitingGame();
    }
    else {
        CRITICAL_MSG("Maximum game data size for synchronization exceeded (max size: 1 GB)");

        // show window
        showWindowLastState(this);
        QMessageBox::critical(nullptr, "Ошибка синхронизации", "Размер данных сохранений " + game->name() + " превышает максимальный допустимый размер "
            "(максимальный допустимый размер: 1 ГБ)");

        game->setSyncMode(SyncMode::SyncFailed);
    }
}

void SavSyncer::syncAllGame() {
    if (!profile->isOnline()) {
        WARNING_MSG("User is not authorized");

        // show window
        showWindowLastState(this);

        QMessageBox::warning(nullptr, "Предупреждение", "Необходимо войти в учетную запись.");
        return;
    }

    for (auto itemList : listGame) {
        if (!itemList->isBusy() && itemList->isSync()) {
            itemList->setSyncMode(SyncMode::SyncProcess);
            INFO_MSG("Sync "+ itemList->name() + " from queue");
            emit startSyncGame(itemList);
        }
    }
}

void SavSyncer::deleteGame(Game* game) {

    if (!profile->isOnline()) {
        WARNING_MSG("User is not authorized");
        QMessageBox::warning(nullptr, "Предупреждение", "Необходимо войти в учетную запись.");
        return;
    }

    if (!game->isBusy()) {
        game->setSyncMode(SyncMode::SyncDeleteProcess);

        // if another game is deleting, then put in queue
        if (!listDeleteGame.isEmpty()) {
            if (listDeleteGame.first() != game) {
                if (!listDeleteGame.contains(game))
                    listDeleteGame.append(game);
                return;
            }
        }
        else
            listDeleteGame.append(game);

        game->setBusy(true);

        // remove from .ss
        QJsonDocument documentJSON(docServiceFile);
        QJsonObject objectJSON = documentJSON.object();
        bool found = false;
        QString nameGameSave;
        for (QString& key : objectJSON.keys()) {
            if (key.toInt() == game->id()) {
                nameGameSave = objectJSON.value(key).toObject()["name_gamesave"].toString();
                objectJSON.remove(key);
                found = true;
                break;
            }
        }
        if (found) {

            // delete save
            bool fileExistence = profile->checkFileExistence("", nameGameSave);
            if (profile->error()) {
                game->setBusy(false);
                game->setSyncMode(SyncMode::SyncDeleteFailed);
                CRITICAL_MSG("Could not find file \".ss\"");
                emit deleteWaitingGame();
                return;
            }
            if (fileExistence) {
                profile->deleteGameData(nameGameSave);
                if (profile->error()) {
                    game->setBusy(false);
                    game->setSyncMode(SyncMode::SyncDeleteFailed);
                    CRITICAL_MSG("Delete file " + nameGameSave + " failed");
                    emit deleteWaitingGame();
                    return;
                }
            }

            // delete game from service file .ss
            QJsonDocument newDoc(objectJSON);
            QByteArray dataServiceFile = newDoc.toJson();
            dataServiceFile = dataServiceFile.toBase64();
            profile->uploadGameData(".ss", dataServiceFile);
            if (profile->error()) {
                game->setBusy(false);
                game->setSyncMode(SyncMode::SyncDeleteFailed);
                CRITICAL_MSG("Upload service file \".ss\" failed");
                emit deleteWaitingGame();
                return;
            }

            // save new service file .ss
            docServiceFile = QJsonDocument(newDoc);
        }

        game->setBusy(false);
        selectGame(nullptr);
        listGame.removeOne(game);
        game->deleteLater();
        emit deleteWaitingGame();
    }
    else {
        CRITICAL_MSG("Unable to delete " + game->name() + "! The game is currently busy");
        QMessageBox::critical(nullptr, "Ошибка", "Невозможно удалить игру! В данный момент игра занята другим процессом.");
    }
}

void SavSyncer::selectGame(Game* game) {
    if (game) {
        if (selectedGame)
            selectedGame->setSelected(false);
        selectedGame = game;
        selectedGame->setSelected(true);
    }
    else {
        if (selectedGame)
            selectedGame->setSelected(false);
        selectedGame = nullptr;
    }
}

void SavSyncer::syncNextGame() {
    if (!listSyncGame.isEmpty()) {
        listSyncGame.takeFirst();
        if (!listSyncGame.isEmpty()) {
            Game* game = listSyncGame.first();
            if (game)
                emit startSyncGame(game);
            else
                emit syncWaitingGame();
        }
    }
}

void SavSyncer::deleteNextGame() {
    if (!listDeleteGame.isEmpty()) {
        listDeleteGame.takeFirst();
        if (!listDeleteGame.isEmpty()) {
            Game* game = listDeleteGame.first();
            if (game)
                deleteGame(game);
            else
                emit deleteWaitingGame();
        }
    }
}

void SavSyncer::backup() {
    INFO_MSG("Start backup");
    QString pathAppData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    pathAppData += "/" + profile->serviceName() + "/" + profile->getId();
    QDir dir(pathAppData);
    if (!dir.exists())
        dir.mkpath(pathAppData);

    QString nameFile = pathAppData + "/backup_" + QDateTime::currentDateTime().toString("yyyy_MM_dd_hh-mm-ss-zzz") + ".ss";
    regSettings->setValue("backup", nameFile);

    QFile file(nameFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(docServiceFile.toJson().toBase64());
        file.close();
        INFO_MSG("Backup completed successfully");
    }
    else
        INFO_MSG("Backup failed");
}

QByteArray SavSyncer::loadBackup() {
    QByteArray output;

    QString pathAppData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    pathAppData += "/" + profile->serviceName() + "/" + profile->getId();
    QString nameFile = regSettings->value("backup").toString();
    QFile file(nameFile);
    QFileInfo fileInfo(file);
    if (fileInfo.exists()) {
        if (file.open(QIODevice::ReadOnly)) {
            output = file.readAll();
            file.close();
        }
    }

    return output;
}

void SavSyncer::removeBackup() {
    QString nameFile = regSettings->value("backup").toString();
    regSettings->setValue("backup", "");
    QFile file(nameFile);
    QFileInfo fileInfo(file);
    if (fileInfo.exists())
        file.remove();
}

void SavSyncer::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {

        // click on empty area
        QPoint cursorPoint = event->globalPos();
        QPoint widgetPointStart = ui.frameGameList->mapToGlobal(ui.verticalSpacer->geometry().topLeft());
        QPoint widgetPointEnd = ui.frameGameList->mapToGlobal(ui.verticalSpacer->geometry().bottomRight());
        if (cursorPoint.x() > widgetPointStart.x() && cursorPoint.x() < widgetPointEnd.x() &&
            cursorPoint.y() > widgetPointStart.y() && cursorPoint.y() < widgetPointEnd.y()) {
            selectGame(nullptr);
        }

        // dragging
        widgetPointStart = ui.frameHeader->mapToGlobal(ui.frameHeader->geometry().topLeft());
        widgetPointEnd = ui.frameHeader->mapToGlobal(ui.frameHeader->geometry().bottomRight());
        if (cursorPoint.x() > widgetPointStart.x() && cursorPoint.x() < widgetPointEnd.x() &&
            cursorPoint.y() > widgetPointStart.y() + 5 && cursorPoint.y() < widgetPointEnd.y()) {
            dragging = true;
            dragStartPosition = event->globalPos() - frameGeometry().topLeft();
        }

        // resize window
        if (cursor().shape() == Qt::SizeHorCursor || cursor().shape() == Qt::SizeVerCursor) {
            if (cursorPoint.y() > geometry().top() && cursorPoint.y() < geometry().top() + indentResize)
                resizingHeightTop = true;
            else if (cursorPoint.y() < geometry().bottom() && cursorPoint.y() > geometry().bottom() - indentResize)
                resizingHeightBottom = true;
            else if (cursorPoint.x() > geometry().left() && cursorPoint.x() < geometry().left() + indentResize)
                resizingWidthLeft = true;
            else if (cursorPoint.x() < geometry().right() && cursorPoint.x() > geometry().right() - indentResize)
                resizingWidthRigth = true;

            lastMousePosition = event->globalPos();
            lastSize = size();
        }

        event->accept();
    }
}

void SavSyncer::mouseMoveEvent(QMouseEvent* event) {

    // dragging
    if (dragging) {
        move(event->globalPos() - dragStartPosition);
        event->accept();
    }

    // resize window
    if (cursor().shape() == Qt::SizeHorCursor || cursor().shape() == Qt::SizeVerCursor) {
        if (resizingHeightTop) {
            int deltaY = event->globalY() - lastMousePosition.y();
            if ((lastSize.height() - deltaY) > minimumHeight() && fabs(lastMousePosition.y() - geometry().top()) < 10) {
                resize(lastSize.width(), lastSize.height() - deltaY);
                move(geometry().left(), geometry().top() + deltaY);
            }
        }
        if (resizingHeightBottom) {
            int deltaY = event->globalY() - lastMousePosition.y();
            if ((lastSize.height() + deltaY) > minimumHeight() && fabs(lastMousePosition.y() - geometry().bottom()) < 10)
                resize(lastSize.width(), lastSize.height() + deltaY);
        }
        if (resizingWidthLeft) {
            int deltaX = event->globalX() - lastMousePosition.x();
            if ((lastSize.width() - deltaX) > minimumWidth() && fabs(lastMousePosition.x() - geometry().left()) < 10) {
                resize(lastSize.width() - deltaX, lastSize.height());
                move(geometry().left() + deltaX, geometry().top());
            }
        }
        if (resizingWidthRigth) {
            int deltaX = event->globalX() - lastMousePosition.x();
            if ((lastSize.width() + deltaX) > minimumWidth() && fabs(lastMousePosition.x() - geometry().right()) < 10)
                resize(lastSize.width() + deltaX, lastSize.height());
        }

        lastSize = size();
        lastMousePosition = event->globalPos();
        event->accept();
    }
}

void SavSyncer::mouseReleaseEvent(QMouseEvent* event) {

    // clear events
    if (event->button() == Qt::LeftButton) {
        resizingHeightTop = false;
        resizingHeightBottom = false;
        resizingWidthLeft = false;
        resizingWidthRigth = false;
        dragging = false;
        event->accept();
    }
}

bool SavSyncer::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::HoverMove) {

        // custom style for widget searching game
        QPoint cursorPoint = cursor().pos();
        QPoint widgetPointStart = ui.btnSearchGame->mapToGlobal(ui.btnSearchGame->geometry().topLeft());
        QPoint widgetPointEnd = ui.btnSearchGame->mapToGlobal(ui.btnClearSearchGame->geometry().bottomRight());
        if (cursorPoint.x() > widgetPointStart.x() - 5 && cursorPoint.x() < widgetPointEnd.x() &&
            cursorPoint.y() > widgetPointStart.y() - 5 && cursorPoint.y() < widgetPointEnd.y()) {
            if (styleSheetLineEditSearchGame.isEmpty()) {
                styleSheetLineEditSearchGame = ui.lineEditSearchGame->styleSheet();
                ui.lineEditSearchGame->setStyleSheet(ui.lineEditSearchGame->styleSheet() +
                    "#lineEditSearchGame{"
                    "background-color: rgb(100, 100, 100);"
                    "}"
                );

                styleSheetBtnSearchGame = ui.btnSearchGame->styleSheet();
                ui.btnSearchGame->setStyleSheet(ui.btnSearchGame->styleSheet() +
                    "#btnSearchGame{"
                    "background-color: rgb(100, 100, 100);"
                    "}"
                );

                styleSheetBtnClearSearchGame = ui.btnClearSearchGame->styleSheet();
                ui.btnClearSearchGame->setStyleSheet(ui.btnClearSearchGame->styleSheet() +
                    "#btnClearSearchGame{"
                    "background-color: rgb(100, 100, 100);"
                    "}"
                );
            }
        }
        else {
            ui.lineEditSearchGame->setStyleSheet(styleSheetLineEditSearchGame);
            styleSheetLineEditSearchGame = "";

            ui.btnSearchGame->setStyleSheet(styleSheetBtnSearchGame);
            styleSheetBtnSearchGame = "";

            ui.btnClearSearchGame->setStyleSheet(styleSheetBtnClearSearchGame);
            styleSheetBtnClearSearchGame = "";
        }

        // show resize cursor
        if ((cursorPoint.y() > geometry().top() && cursorPoint.y() < geometry().top() + indentResize) ||
            (cursorPoint.y() < geometry().bottom() && cursorPoint.y() > geometry().bottom() - indentResize)) {
            if(!(resizingHeightTop || resizingHeightBottom || resizingWidthLeft || resizingWidthRigth))
                setCursor(Qt::CursorShape::SizeVerCursor);
        }
        else if ((cursorPoint.x() > geometry().left() && cursorPoint.x() < geometry().left() + indentResize) ||
            (cursorPoint.x() < geometry().right() && cursorPoint.x() > geometry().right() - indentResize)) {
            if (!(resizingHeightTop || resizingHeightBottom || resizingWidthLeft || resizingWidthRigth))
                setCursor(Qt::CursorShape::SizeHorCursor);
        }
        else {
            if (!(resizingHeightTop || resizingHeightBottom || resizingWidthLeft || resizingWidthRigth))
                setCursor(Qt::CursorShape::ArrowCursor);
        }
    }

    return false;
}

void SavSyncer::showEvent(QShowEvent* event) {
    trayIcon->hide();
}

void SavSyncer::closeEvent(QCloseEvent* event) {
    if (settings->getBackgroundWork() && !forceClose) {
        event->ignore();
        hide();
        if (about)
            about->hide();
        if (profile)
            profile->hide();
        if (settings)
            settings->hide();
        trayIcon->show();
    }
    else {
        QMessageBox msgClose(QMessageBox::Question, "Выход", "Вы действительно хотите выйти?", QMessageBox::Yes | QMessageBox::Cancel);
        auto yesBtn = msgClose.button(QMessageBox::Yes);
        yesBtn->setText("Да");
        auto cancelBtn = msgClose.button(QMessageBox::Cancel);
        cancelBtn->setText("Отмена");
        if (!listSyncGame.isEmpty() || !listDeleteGame.isEmpty()) {
            msgClose.setText("В данный момент идет синхронизация. Вы действительно хотите выйти?");
            msgClose.setDefaultButton(QMessageBox::Cancel);
            int res = msgClose.exec();
            if (res != QMessageBox::Yes) {
                event->ignore();
                return;
            }
            else
                qApp->quit();
        }
        else {
            int res = msgClose.exec();
            if (res != QMessageBox::Yes) {
                event->ignore();
                return;
            }
        }

        if (about)
            about->close();
        if (profile)
            profile->close();
        if (settings)
            settings->close();
        QMainWindow::closeEvent(event);
    }
}

void SavSyncer::hideEvent(QHideEvent* event) {
    if(!isMinimized())
        trayIcon->show();
    QWidget::hideEvent(event);
}

bool SavSyncer::nativeEvent(const QByteArray& eventType, void* message, long* result) {
    MSG* msg = static_cast<MSG*>(message);
    
    // restart os
    /*if (msg->message == WM_QUERYENDSESSION) {
        
    }*/

    //if (msg->message == WM_QUERYENDSESSION) {
    //    // Обработка события завершения сеанса
    //    QMessageBox::StandardButton resBtn = QMessageBox::question(this, "Подтверждение",
    //        "Вы действительно хотите завершить сеанс?");

    //    if (resBtn != QMessageBox::Yes) {
    //        *result = 0; // Отменяем завершение сеанса
    //        return true; // Событие обработано
    //    }
    //}

    return QMainWindow::nativeEvent(eventType, message, result);
}