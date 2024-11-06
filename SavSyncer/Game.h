#pragma once

#include "defines.h"

#include <QObject>
#include <QDateTime>
#include <QThread>
#include <QtEndian>
#include <QFileIconProvider>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <tlhelp32.h>
#endif

#include "ItemGameList.h"
#include "PageGameList.h"

class Game : public QObject
{
	Q_OBJECT

public:
	Game(ItemGameList* item, PageGameList* page, QSettings* regset);
	~Game();

	bool isSync();
	void setSyncEnable(bool value);
	bool isValid();
	bool isBusy();
	void setBusy(bool value);

	void setId(unsigned int id);
	void setName(QString name);
	void setFilePath(QString path);
	void setIcon(QIcon icon);
	void setVersion(QString text);
	void setSyncMode(SyncMode syncMode);
	void setSelected(bool value);
	void setPathSave(QString path);
	void setDateLastSync(QDateTime date);

	unsigned int id();
	QString name();
	QString filePath();
	QIcon icon();
	QString version();
	SyncMode syncMode();
	QString pathGameSave();
	long long saveSize();
	QDateTime dateLastModified();
	QDateTime dateLastSync();
	QDateTime dateLastRun();

	bool startMonitoringApp();
	void stopMonitoringApp();

public slots:

	void showItem();
	void hideItem();

	void updateGameInfo();
	void updateGameSaveInfo();

signals:

	void clickedItemMouseLeftButton();
	void clickedItemMouseRightButton();
	void closedGame();
	void enabledSync(bool value);

private:

	long long getFolderSize(const QString& folderPath);
	QDateTime getFolderLastModified(const QString& folderPath);
	bool isProcessRunning(const QString& processName);

private:

	QSettings* regSettings = nullptr;
	unsigned int m_gameId = 0;

	ItemGameList* m_item = nullptr;
	PageGameList* m_page = nullptr;

	bool busy = false;

	long long m_saveSize;
	QDateTime m_dateLastModified;
	QDateTime m_dateLastSync;
	QDateTime m_dateLastRun;

	QTimer* timerMonitoring = nullptr;
	bool flagGameHasBeenRun = false;

	QByteArray winStringFileInfo(QString path);
	QString winLangNumber(QByteArray* stringFileInfo);
	QString winStringFileInfoProperty(QString path, QString nameProperty);
};
