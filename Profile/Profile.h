#pragma once

#include "defines.h"
#include "IServices.h"

#include <QWidget>
#include "ui_Profile.h"

#include <QSettings>
#include <QPluginLoader>
#include <QDir>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMessageBox>

enum Status {
	Unknown = 0,
	Offline = 1,
	Online = 2
};

class Profile : public QWidget
{
	Q_OBJECT

public:
	Profile(QSettings* regset, QWidget *parent = nullptr);
	~Profile();

	QString serviceName();

	bool error();
	bool isOnline();

	void signIn();
	void signOut();

	void loadUserData();
	QString getId();
	QString getFirstName();
	QString getLastName();
	QPixmap getAvatar();

	bool checkFileExistence(QString path, QString nameFile);
	QStringList listFiles(QString path);
	void loadGameDataInfo(QString path);
	QDateTime lastModifiedGameData();

	void uploadGameData(QString path, QByteArray compressed);
	QByteArray downloadGameData(QString path);
	void deleteGameData(QString path);

signals:

	void signalSuccessfulSignIn();
	void signalFailedSignIn();
	void signalSignOut();

private slots:

	void createWidgetAuthorization(IServices* service);

	void clickedAuthorization(IServices* service);

	void showProfile(Status status);

private:
	Ui::Profile ui;
	QSettings *regSettings = nullptr;

	QList<IServices*> services;
	IServices* lastAuthService = nullptr;

	bool flagOnline = false;
};
