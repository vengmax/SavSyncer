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

private:
	Ui::About ui;
	QSettings* regSettings = nullptr;
};
