#pragma once

#include "defines.h"

#include <QWidget>
#include "ui_About.h"

#include <QSettings>

class About : public QWidget
{
	Q_OBJECT

public:
	About(QSettings* regset, QWidget *parent = nullptr);
	~About();

	void setVersion(QString version);
	void setBuildDate(QString buildDate);
	void setVersionAPI(QString versionAPI);
	void setContact(QString contact);

	QString version();
	QString buildDate();
	QString versionAPI();
	QString contact();

private:
	Ui::About ui;
	QSettings* regSettings = nullptr;
};
