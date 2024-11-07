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

    // tray
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/logo.png"));
    trayIcon->setToolTip("SavSyncer");
    QMenu* menuTrayIcon = new QMenu(this);
    menuTrayIcon->addAction("Показать", [this]() {
        show();
        raise();
        activateWindow();
        });
    menuTrayIcon->addAction("Синхронизировать", [this]() {
        syncAllGame();
        });
    menuTrayIcon->addAction("Выйти", [this]() {
        forceClose = true;
        close();
        });
    trayIcon->setContextMenu(menuTrayIcon);
    connect(trayIcon, &QSystemTrayIcon::activated, this, [this, menuTrayIcon](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::ActivationReason::Trigger) {
            show();
            raise();
            activateWindow();
        }
        });
    trayIcon->show();

    // menu logo
    QMenu* menuLogo = new QMenu(this);
    actionAuth = menuLogo->addAction("Войти", profile, SLOT(show()));
    menuLogo->addSeparator();
    menuLogo->addAction("Настройки", settings, SLOT(show()));
    menuLogo->addSeparator();
    menuLogo->addAction("Выход", [this]() {
        forceClose = true;
        close();
        });
    connect(ui.btnLogo, &QPushButton::clicked, [this, menuLogo]() {
        menuLogo->exec(ui.btnLogo->mapToGlobal(QPoint(0, ui.btnLogo->height())));
        });

    // menu reference
    QMenu* menuReference = new QMenu(this);
    menuReference->addAction("Служба поддержки", about, SLOT(show()));
    menuReference->addSeparator();
    menuReference->addAction("О SavSyncer", about, SLOT(show()));
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
    connect(ui.btnAuth, SIGNAL(clicked()), profile, SLOT(show()));
    connect(ui.btnAuth, SIGNAL(clicked()), profile, SLOT(raise()));
    connect(ui.btnAuth, SIGNAL(clicked()), profile, SLOT(showNormal()));

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
    if (msgSignOut)
        delete msgSignOut;
}

void SavSyncer::onAboutToQuit() {
    if (!listSyncGame.isEmpty() || !listDeleteGame.isEmpty())
        backup();
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
        profile->uploadGameData(".ss", backupData);
        if (profile->error()) {
            CRITICAL_MSG("Upload backup service file \".ss\" failed");

            // show window
            if (isMinimized())
                showNormal();
            show();
            raise();

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
    if (profile->checkFileExistence(".ss")) {
        QByteArray jsonData = profile->downloadGameData(".ss");
        if (profile->error()) {
            CRITICAL_MSG("Download service file \".ss\" failed");
            QApplication::restoreOverrideCursor();

            // show window
            if (isMinimized())
                showNormal();
            show();
            raise();

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
                if (isMinimized())
                    showNormal();
                show();
                raise();

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
        game->setSyncMode(SyncMode::SyncDeleteProcess);
        deleteGame(game);
        });

    connect(game, &Game::clickedItemMouseRightButton, [this, game, menuItemGameList]() {
        selectGame(game);
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

QByteArray SavSyncer::serializeFolder(const QString& folderPath) {
    INFO_MSG("Packing data for synchronization");

    QJsonObject folderObject;
    QDir dir(folderPath);
    foreach(const QString & fileName, dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QString filePath = dir.absoluteFilePath(fileName);
        if (QFileInfo(filePath).isFile()) {
            QJsonObject fileObject;
            fileObject["size"] = QFileInfo(filePath).size(); // Добавьте другие метаданные
            QFile file(filePath);
            QByteArray fileData;
            if (file.open(QIODevice::ReadOnly)) {
                fileData = file.readAll();
                file.close();
            }
            fileObject["data"] = QString(fileData.toBase64());
            folderObject["file:" + fileName] = fileObject;
        }
        else {
            QJsonDocument subfolderObject = QJsonDocument::fromJson(serializeFolder(filePath));
            folderObject["folder:" + fileName] = subfolderObject.object();
        }
    }
    QJsonDocument document(folderObject);
    return document.toJson();
}

void SavSyncer::deserializeFolder(const QByteArray& jsonData, const QString& targetPath) {
    INFO_MSG("Unpacking data for synchronization");

    QJsonDocument documentJSON = QJsonDocument::fromJson(jsonData);
    QJsonObject objectJSON = documentJSON.object();
    if (documentJSON.isNull() || !documentJSON.isObject()) {
        CRITICAL_MSG("Data is not a JSON file");
        return;
    }

    QDir targetDir;
    targetDir.mkpath(targetPath);
    if (!targetDir.exists()) {
        CRITICAL_MSG("Failed to create folder");
        return;
    }

    for (QString &key: objectJSON.keys()){
        if (key.startsWith("folder:")) {
            QByteArray data = QJsonDocument(objectJSON.value(key).toObject()).toJson();
            deserializeFolder(data, targetPath + "/" + key.remove("folder:"));
        }
        else if (key.startsWith("file:")) {
            QJsonObject fileObject = objectJSON.value(key).toObject();
            if (fileObject.contains("data") && fileObject.contains("size")) {

                QByteArray base64Data = fileObject["data"].toString().toUtf8();
                QByteArray data = QByteArray::fromBase64(base64Data);

                QFile file(targetPath + "/" + key.remove("file:"));
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(data);
                    file.close();
                }
                else {
                    CRITICAL_MSG("Cannot open file for writing");
                    return;
                }
            }
            else {
                CRITICAL_MSG("Data is not a JSON file");
                return;
            }
        }
        else {
            CRITICAL_MSG("Data is not a JSON file");
            return;
        }
    }
}

void SavSyncer::syncGame(Game *game) {

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

    if (game->saveSize() < (long long)settings->getMaxSyncSize() * 1024 * 1024) {

        // if another game is syncing, then put in queue
        if (!listSyncGame.isEmpty()) {
            if (listSyncGame.first() != game) {
                if(!listSyncGame.contains(game))
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
        game->updateGameSaveInfo();
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
                if (jsonGameInfo["name"].toString() != game->name() || jsonGameInfo["path_game"].toString() != game->filePath() ||
                    jsonGameInfo["path_gamesave"].toString() != game->pathGameSave()) 
                {
                    QMessageBox msg;
                    msg.setIcon(QMessageBox::Information);
                    msg.setWindowTitle("Синхронизация данных");
                    msg.setWindowFlags(msg.windowFlags() & ~Qt::AA_DisableWindowContextHelpButton);
                    msg.setText("Были изменены данные игры " + game->name() + ". Для продолжения выберите где находятся актуальные данные.");
                    msg.setStandardButtons(QMessageBox::Yes);
                    msg.addButton(QMessageBox::No);
                    msg.addButton(QMessageBox::Cancel);
                    msg.setDefaultButton(QMessageBox::Yes);
                    QAbstractButton* yesButton = msg.button(QMessageBox::Yes);
                    yesButton->setText("На диске");
                    QAbstractButton* noButton = msg.button(QMessageBox::No);
                    noButton->setText("На компьютере");
                    noButton->setMinimumWidth(120);
                    QAbstractButton* cancelButton = msg.button(QMessageBox::Cancel);
                    cancelButton->setText("Отмена");
                    int res = msg.exec();
                    if (res == QMessageBox::Rejected || res == QMessageBox::Cancel) {
                        game->setBusy(false);
                        game->setSyncMode(SyncMode::SyncUncompleted);
                        emit syncWaitingGame();
                        return;
                    }
                    else if (res == QMessageBox::Yes)
                        downloadSave = true;
                    else if (res == QMessageBox::No)
                        uploadSave = true;
                    else {
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

                // create .ss
                QJsonObject jsonObjectGameData;
                jsonObjectGameData["name"] = game->name();
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
                    game->setBusy(false);
                    game->setSyncMode(SyncMode::SyncFailed);
                    CRITICAL_MSG(game->name() + ": Upload service file \".ss\" failed");
                    emit syncWaitingGame();
                    return;
                }

                // upload game save
                auto funcCompress = [this, path, &data, &eveloop, game]() {
                    INFO_MSG("Start serialize and compress data " + game->name());
                    data = serializeFolder(path);
                    data = qCompress(data, 4);
                    INFO_MSG("Finished serialize and compress data " + game->name());
                    eveloop.quit();
                    };
                if (QMetaObject::invokeMethod(objectAnotherThread, funcCompress, Qt::QueuedConnection))
                    eveloop.exec();

                profile->uploadGameData(nameGameSave, data);
                if (profile->error()) {
                    game->setBusy(false);
                    game->setSyncMode(SyncMode::SyncFailed);
                    CRITICAL_MSG("Synchronization " + game->name() + " failed");
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
                    game->setBusy(false);
                    game->setSyncMode(SyncMode::SyncFailed);
                    CRITICAL_MSG("Synchronization " + game->name() + " failed");
                    emit syncWaitingGame();
                    return;
                }

                auto funcUncompress = [this, path, &data, &eveloop, game]() {
                    INFO_MSG("Start Uncompress and deserialize data " + game->name());
                    data = qUncompress(data);
                    deserializeFolder(data, path);
                    INFO_MSG("Finished uncompress and deserialize data " + game->name());
                    eveloop.quit();
                    };
                if (QMetaObject::invokeMethod(objectAnotherThread, funcUncompress, Qt::QueuedConnection))
                    eveloop.exec();

                // update info game save
                game->updateGameSaveInfo();
                lastLocalGameDataModified = game->dateLastModified();

                // create .ss
                QJsonObject jsonObjectGameData;
                jsonObjectGameData["name"] = game->name();
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
                    game->setBusy(false);
                    game->setSyncMode(SyncMode::SyncFailed);
                    CRITICAL_MSG(game->name() + ": Upload service file \".ss\" failed");
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
        CRITICAL_MSG("Maximum data size for synchronization exceeded");
        game->setSyncMode(SyncMode::SyncFailed);
        long long maxSaveSize = (long long)settings->getMaxSyncSize() * 1024 * 1024;
        QString strMaxSaveSize;
        if (maxSaveSize < 1LL * 1024 * 1024)
            strMaxSaveSize = QString::number(maxSaveSize / (double)(1024), 'f', 1) + " КБ";
        else if (maxSaveSize < 1LL * 1024 * 1024 * 1024)
            strMaxSaveSize = QString::number(maxSaveSize / (double)(1024 * 1024), 'f', 1) + " МБ";
        else if (maxSaveSize < 1LL * 1024 * 1024 * 1024 * 1024)
            strMaxSaveSize = QString::number(maxSaveSize / (double)(1LL * 1024 * 1024 * 1024), 'f', 1) + " ГБ";
        else
            strMaxSaveSize = QString::number(maxSaveSize / (double)(1LL * 1024 * 1024 * 1024 * 1024), 'f', 1) + " ТБ";
        //QMessageBox::critical(nullptr, "Ошибка синхронизации", "Максимальный размер данных " + strMaxSaveSize);
    }
}

void SavSyncer::syncAllGame() {
    if (!profile->isOnline()) {
        WARNING_MSG("User is not authorized");
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
    if (!game->isBusy()) {

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
            if (profile->checkFileExistence(nameGameSave)) {
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
        //QMessageBox::critical(nullptr, "Ошибка", "Невозможно удалить игру! В данный момент игра занята другим процессом.");
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
    listSyncGame.takeFirst();
    if (!listSyncGame.isEmpty()) {
        Game* game = listSyncGame.first();
        if (game)
            emit startSyncGame(game);
        else
            emit syncWaitingGame();
    }
}

void SavSyncer::deleteNextGame() {
    listDeleteGame.takeFirst();
    if (!listDeleteGame.isEmpty()) {
        Game* game = listDeleteGame.first();
        if (game)
            deleteGame(game);
        else
            emit deleteWaitingGame();
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
    }
    else {
        hide();
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