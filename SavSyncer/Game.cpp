#include "Game.h"

Game::Game(ItemGameList* item, PageGameList* page, QSettings* regset){
	m_item = item;
	m_page = page;
	regSettings = regset;
	
	connect(m_item, SIGNAL(clickedMouseLeftButton()), this, SIGNAL(clickedItemMouseLeftButton()));
	connect(m_item, SIGNAL(clickedMouseRightButton()), this, SIGNAL(clickedItemMouseRightButton()));

	connect(m_page, SIGNAL(refreshGame()), this, SLOT(refresh()));
	connect(m_page, SIGNAL(refreshGameInfo()), this, SLOT(refreshInfo()));
	connect(m_page, &PageGameList::enabledSync, this, [this](bool value) {
		if(value)
			m_item->setSyncMode(SyncMode::SyncUncompleted);
		else
			m_item->setSyncMode(SyncMode::SyncDisabled);
		emit enabledSync(value);
		});
	connect(m_page, &PageGameList::validPath, this, [this](bool value) {
		m_item->setSyncMode(SyncMode::SyncUncompleted);
		if (value) {
			if(m_page->isValidPathGame() && m_page->isValidPathGameSave())
				m_item->setValid(true);
			else
				m_item->setValid(false);
		}
		else
			m_item->setValid(false);
		});

	timerMonitoring = new QTimer(this);
}

Game::~Game(){
	timerMonitoring->stop();
	timerMonitoring->deleteLater();
	m_item->deleteLater();
	dynamic_cast<QStackedWidget*>(m_page->parent())->removeWidget(m_page);
	m_page->deleteLater();
}

bool Game::isSync() {
	return m_page->isSync();
}
void Game::setSyncEnable(bool value) {
	m_page->setSyncEnable(value);
}
bool Game::isValid() {
	return (m_page->isValidPathGame() && m_page->isValidPathGameSave() && m_item->isValid());
}
bool Game::isBusy() {
	return busy;
}
void Game::setBusy(bool value) {
	busy = value;
	m_page->setDisabled(value);
}

void Game::setId(unsigned int id) {
	m_gameId = id;
}
void Game::setName(QString name) {
	m_item->setName(name);
	m_page->setTitle(name);
}
void Game::setFilePath(QString path) {
	m_page->setPathGame(path);
}
void Game::setIcon(QIcon icon) {
	m_item->setIcon(icon);
}
void Game::setVersion(QString text) {
	m_item->setVersion(text);
}
void Game::setSyncMode(SyncMode syncMode) {
	m_item->setSyncMode(syncMode);
}
void Game::setSelected(bool value) {
	m_item->setSelected(value);
	QStackedWidget* wid = dynamic_cast<QStackedWidget*>(m_page->parent());
	if (value)
		wid->setCurrentWidget(m_page);
	else
		wid->setCurrentIndex(0);
}
void Game::setPathSave(QString path) {
	m_page->setPathGameSave(path);
}
void Game::setDateLastSync(QDateTime date) {
	m_dateLastSync = date;
	m_page->setGameSaveLastSync(date.toString("dd.MM.yyyy hh:mm:ss"));
}

unsigned int Game::id() {
	return m_gameId;
}
QString Game::name() {
	return m_item->name();
}
QString Game::filePath() {
	return m_page->pathGame();
}
QIcon Game::icon() {
	return m_item->icon();
}
QString Game::version() {
	return m_item->version();
}
SyncMode Game::syncMode() {
	return m_item->syncMode();
}
QString Game::pathGameSave() {
	return m_page->pathGameSave();
}
long long Game::saveSize() {
	return m_saveSize;
}
QDateTime Game::dateLastModified() {
	return m_dateLastModified;
}
QDateTime Game::dateLastSync() {
	return m_dateLastSync;
}
QDateTime Game::dateLastRun() {
	return m_dateLastRun;
}

bool Game::startMonitoringApp() {
	if (!timerMonitoring->isActive()) {
		timerMonitoring = new QTimer();
		
		connect(timerMonitoring, &QTimer::timeout, [this]() {
			if (m_page->isValidPathGame()) {
				QString nameFile = QFileInfo(m_page->pathGame()).fileName();

				if (isProcessRunning(nameFile))
					flagGameHasBeenRun = true;
				else {
					if (flagGameHasBeenRun) {
						flagGameHasBeenRun = false;
						emit closedGame();
					}
				}
			}
			});
		timerMonitoring->start(7000);
		return true;
	}
	else
		return false;
}
void Game::stopMonitoringApp() {
	flagGameHasBeenRun = false;
	timerMonitoring->stop();
}

void Game::showItem() {
	m_item->show();
}

void Game::hideItem() {
	m_item->hide();
}

void Game::refresh() {

	QString path = m_page->pathGame();
	QFileInfo info(path);

	// name
	QString gameName = winStringFileInfoProperty(path, "FileDescription");
	if (gameName.isEmpty())
		setName(info.fileName());
	else
		setName(gameName);

	// version
	QString gameVersion = winStringFileInfoProperty(path, "ProductVersion");
	if (gameVersion.isEmpty())
		setVersion("-");
	else
		setVersion(gameVersion);

	// icon
	QFileIconProvider iconProvider;
	QIcon icon = iconProvider.icon(info);
	setIcon(icon);
}

void Game::refreshInfo() {
	
	QApplication::setOverrideCursor(Qt::CursorShape::BusyCursor);

	// Save size
	if (m_page->isValidPathGameSave()) {
		m_saveSize = getFolderSize(m_page->pathGameSave());
		QString strSaveSize;
		if (m_saveSize < 1LL * 1024 * 1024)
			strSaveSize = QString::number(m_saveSize / (double)(1024), 'f', 1) + " КБ";
		else if (m_saveSize < 1LL * 1024 * 1024 * 1024)
			strSaveSize = QString::number(m_saveSize / (double)(1024 * 1024), 'f', 1) + " МБ";
		else
			strSaveSize = QString::number(m_saveSize / (double)(1LL * 1024 * 1024 * 1024), 'f', 1) + " ГБ";
		m_page->setGameSaveSize(strSaveSize);

		// Last modified
		m_dateLastModified = getFolderLastModified(m_page->pathGameSave());
		m_page->setGameSaveLastModified(m_dateLastModified.toString("dd.MM.yyyy hh:mm:ss"));
	}
	else {
		m_page->setGameSaveSize("???");
		m_page->setGameSaveLastModified("???");
	}

	// Last run
	if (m_page->isValidPathGame()) {
		QFileInfo fileInfo(m_page->pathGame());
		m_dateLastRun = fileInfo.lastRead();
		m_page->setGameLastRun(m_dateLastRun.toString("dd.MM.yyyy hh:mm:ss"));
	}
	else {
		m_page->setGameLastRun("???");
	}

	QApplication::restoreOverrideCursor();
}

long long Game::getFolderSize(const QString& folderPath) {
	QDir dir(folderPath);
	if (!dir.exists())
		return 0;

	qint64 totalSize = 0;
	QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::AllDirs);
	for (const QFileInfo& fileInfo : fileList) {
		if (fileInfo.isDir())
			totalSize += getFolderSize(fileInfo.absoluteFilePath());
		else
			totalSize += fileInfo.size();
	}
	return totalSize;
}

QDateTime Game::getFolderLastModified(const QString& folderPath) {
	QDir dir(folderPath);
	if (!dir.exists())
		return QDateTime();

	QDateTime lastModifiedTime;

	QFileInfoList fileList = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::AllDirs);
	for (const QFileInfo& fileInfo : fileList) {
		if (fileInfo.isDir()) {
			QDateTime subDirModifiedTime = getFolderLastModified(fileInfo.absoluteFilePath());
			if (subDirModifiedTime > lastModifiedTime)
				lastModifiedTime = subDirModifiedTime;
		}
		else {
			if (fileInfo.lastModified() > lastModifiedTime)
				lastModifiedTime = fileInfo.lastModified();
		}
	}

	return lastModifiedTime;
}

bool Game::isProcessRunning(const QString& processName) {
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;

	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == INVALID_HANDLE_VALUE) {
		return false;
	}

	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hProcessSnap, &pe32)) {
		CloseHandle(hProcessSnap);
		return false;
	}

	do {
		if (QString(pe32.szExeFile) == processName) {
			CloseHandle(hProcessSnap);
			return true;
		}
	} while (Process32Next(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);
	return false;
}

QByteArray Game::winStringFileInfo(QString path) {
#ifdef Q_OS_WIN

	DWORD handle;
	LPCSTR pathGameLPCSTR = path.toStdString().c_str();
	DWORD size = GetFileVersionInfoSize(pathGameLPCSTR, &handle);
	if (size == 0) {
		WARNING_MSG("Failed to get stringFileInfo size");
		return QByteArray();
	}

	QByteArray stringFileInfo(size, 0);
	if (!GetFileVersionInfo(pathGameLPCSTR, handle, size, stringFileInfo.data())) {
		WARNING_MSG("Failed to get stringFileInfo information");
		return QByteArray();
	}

	return stringFileInfo;

#endif
	return QByteArray();
}

QString Game::winLangNumber(QByteArray* stringFileInfo) {
#ifdef Q_OS_WIN

	DWORD defaultLangID = GetSystemDefaultLangID();
	QString stringDefaultLangID = QString::number(defaultLangID, 16).rightJustified(4, '0');
	QString stringLangEN = "0409"; // English USA

	unsigned char* lang;
	UINT langSize;
	if (!VerQueryValue(stringFileInfo->data(), "\\VarFileInfo\\Translation", reinterpret_cast<LPVOID*>(&lang), &langSize)) {
		WARNING_MSG("Failed to get language information");
		return QString();
	}

	QString langNumber;
	if (langSize == 4) {
		for (int i = 0; i < langSize / 2; i++) {
			unsigned short value = *reinterpret_cast<unsigned short*>(lang + (i * 2));
			value = qToBigEndian(value);
			langNumber += QString::number(value & 0xff, 16).rightJustified(2, '0').toUpper();
			langNumber += QString::number((value >> 8) & 0xff, 16).rightJustified(2, '0').toUpper();
		}

		QString codeLang = langNumber.mid(0, 4);
		if (codeLang == "0000")
			langNumber.replace(0, 4, stringLangEN);
		langNumber += "\\";
	}
	return langNumber;

#endif
	return QString();
}

QString Game::winStringFileInfoProperty(QString path, QString nameProperty) {
#ifdef Q_OS_WIN

	QByteArray stringFileInfo = winStringFileInfo(path);
	if (stringFileInfo.isEmpty())
		return QString();

	QString langNumber = winLangNumber(&stringFileInfo);
	if (langNumber.isEmpty())
		return QString();

	unsigned char* valueProperty;
	UINT valuePropertySize;
	QString tagValueProperty("\\StringFileInfo\\" + langNumber + nameProperty);
	if (VerQueryValue(stringFileInfo.data(), tagValueProperty.toStdString().c_str(), reinterpret_cast<LPVOID*>(&valueProperty), &valuePropertySize)) {
		QString valuePropertyString = QString::fromLocal8Bit(reinterpret_cast<const char*>(valueProperty), valuePropertySize);
		return valuePropertyString;
	}
	else {
		WARNING_MSG("Failed to get " + nameProperty);
		return QString();
	}

#endif
	return QString();
}