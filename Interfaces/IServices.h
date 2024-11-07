#pragma once

#include <QObject>
#include "defines.h"

// ver api: 0.1 alpha
class IServices : public QObject
{
public:
	virtual ~IServices() = default;

	// name service (example: "Yandex")
	virtual QString name() = 0;

	// button text (example: "sign in via yandex")
	virtual QString widgetText() = 0;

	// button text color
	virtual QColor widgetTextColor() = 0;

	// button background color
	virtual QColor widgetBackgroundColor() = 0;

	// button icon
	virtual QIcon widgetIcon() = 0;

	// has error
	virtual bool error() = 0;

	// http code (example: "404")
	virtual QString httpCode() = 0;

	// http headers
	virtual QMap<QByteArray, QByteArray> httpHeaders() = 0;

public slots:

	// first sign in
	virtual QString authorization() = 0;

	// sign in
	virtual void signIn(QString token) = 0;

	// sign out
	virtual void signOut() = 0;


	// load use info (example: id, login, first name, last name, avatar, storage total space, used space)
	virtual void loadUserInfo() = 0;
	virtual QString userId() = 0;
	virtual QString login() = 0;
	virtual QString firstName() = 0;
	virtual QString lastName() = 0;
	virtual QPixmap avatar() = 0;
	virtual long long storageTotalSpace() = 0;
	virtual long long storageUsedSpace() = 0;

	// check file existence
	virtual bool checkFileExistence(QString path, QString nameFile) = 0;

	// list file names
	virtual QStringList listFileNames(QString path) = 0;

	// load file info (example: last modified)
	virtual void loadFileInfo(QString path) = 0;
	virtual QDateTime lastModified() = 0;

	// work with data
	virtual QByteArray download(QString path) = 0;
	virtual void upload(QString path, QByteArray data) = 0;
	virtual void deleteResource(QString path) = 0;
};

Q_DECLARE_INTERFACE(IServices, "SavSyncer.Services")
