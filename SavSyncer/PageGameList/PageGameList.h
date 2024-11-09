#pragma once

#include "defines.h"

#include <QWidget>
#include "ui_PageGameList.h"

#include <QFileDialog>
#include <QSettings>
#include <QStackedWidget>

class PageGameList : public QWidget
{
	Q_OBJECT

public:
	PageGameList(QSettings* regset, QWidget *parent = nullptr);
	~PageGameList();

	void setTitle(QString text);

	void setPathGame(QString path);
	void setPathGameSave(QString path);
	//void setPathGameSettings(QString path);
	void setGameSaveSize(QString gameSaveSize);
	void setGameSaveLastModified(QString date);
	void setGameSaveLastSync(QString date);
	void setGameLastRun(QString date);

	QString pathGame();
	QString pathGameSave();
	//QString pathGameSettings();
	QString gameSaveSize();
	QString gameSaveLastModified();
	QString gameSaveLastSync();
	QString gameLastRun();

	bool isSync();
	void setSyncEnable(bool value);
	bool isValidPathGame();
	bool isValidPathGameSave();

signals:

	void refreshGame();
	void refreshGameInfo();
	void enabledSync(bool value);
	void validPath(bool value);

private:
	Ui::PageGameList ui;
	QSettings* regSettings = nullptr;

	bool validPathGame = true;
	bool validPathGameSave = true;
};
