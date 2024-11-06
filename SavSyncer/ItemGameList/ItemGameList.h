#pragma once

#include "defines.h"

#include <QWidget>
#include "ui_ItemGameList.h"

#include <QIcon>
#include <QMouseEvent>
#include <QTimer>
#include <QSettings>

enum SyncMode {
	SyncUncompleted = 0,
	SyncCompleted = 1,
	SyncProcess = 2,
	SyncFailed = 3,
	SyncDisabled = 4,
	SyncDeleteProcess = 5,
	SyncDeleteFailed = 6,
};

class ItemGameList : public QWidget
{
	Q_OBJECT

public:
	ItemGameList(QSettings* regset, QWidget *parent = nullptr);
	~ItemGameList();

	bool isValid();

	void setIcon(QIcon icon);
	void setName(QString name);
	void setVersion(QString text);
	void setSyncMode(SyncMode syncMode);

	QIcon icon();
	QString name();
	QString version();
	SyncMode syncMode();

public slots:

	void setValid(bool value);
	void setSelected(bool value);

signals:
	void clickedMouseLeftButton();
	void clickedMouseRightButton();

protected:
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

private:
	Ui::ItemGameList ui;
	QSettings* regSettings = nullptr;

	SyncMode currentMode;

	QTimer timerSync;
	int syncTimerCountPoint = 0;

	bool valid = false;
	bool selected = false;

	QString styleSheetValid;
	QString styleSheetSelected;
};

