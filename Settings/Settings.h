#pragma once

#include "defines.h"

#include <QWidget>
#include "ui_Settings.h"

#include <QSettings>
#include <QTreeWidgetItem>
#include <QStandardPaths>
#include <QDir>

class Settings : public QWidget
{
	Q_OBJECT

public:
	Settings(QSettings* regset, QWidget *parent = nullptr);
	~Settings();

	bool getStartUp();
	bool getStartUpMinimized();
	bool getBackgroundWork();
	bool getAutoSync();
	bool getAutoSignIn();

private slots:

	void on_btnOk_clicked();
	void on_btnApply_clicked();
	void on_btnCancel_clicked();
	void on_btnReset_clicked();

	void on_treeWidgetSettings_itemClicked(QTreeWidgetItem* item, int column);
	void on_lineEditFindSettings_textChanged(const QString& text);

private:

	void setStartUp(bool value, bool minimized);

private:
	Ui::Settings ui;
	QSettings* regSettings = nullptr;

	QString appName = "SavSyncer";

	bool startup = true;
	bool startupMinimized = true;
	bool backgroundWork = true;
	bool defaultStartup = true;
	bool defaultStartupMinimized = true;
	bool defaultBackgroundWork = true;

	bool autoSync = false;
	bool defaultAutoSync = false;

	bool autoSignIn = true;
	bool defaultAutoSignIn = true;

	void showEvent(QShowEvent* event) override;
};
