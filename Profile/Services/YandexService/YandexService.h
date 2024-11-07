#pragma once

#include "defines.h"

#include "IServices.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QWebEngineView>
#include <QWebEngineProfile>
#include <QWebEngineCookieStore>
#include <QMessageBox>
#include <QTextEdit>
#include <QFile>
#include <QEventLoop>
#include <QVBoxLayout>
#include <QSettings>
#include <QIcon>

class YandexService : public IServices
{
	Q_OBJECT
	Q_INTERFACES(IServices)
	Q_PLUGIN_METADATA(IID "SavSyncer.Services.YandexService" FILE "dll.json")

public:
	explicit YandexService();
	~YandexService() override;

	QString name() override;

	QString widgetText() override;
	QColor widgetTextColor() override;
	QColor widgetBackgroundColor() override;
	QIcon widgetIcon() override;

	bool error() override;
	QString httpCode() override;
	QMap<QByteArray, QByteArray> httpHeaders() override;

public slots:

	QString authorization() override;
	void signIn(QString token) override;
	void signOut() override;

	void loadUserInfo() override;
	QString userId() override;
	QString login() override;
	QString firstName() override;
	QString lastName() override;
	QPixmap avatar() override;
	long long storageTotalSpace() override;
	long long storageUsedSpace() override;

	bool checkFileExistence(QString path, QString nameFile) override;
	QStringList listFileNames(QString path) override;
	void loadFileInfo(QString path) override;
	QDateTime lastModified() override;

	QByteArray download(QString path) override;
	void upload(QString path, QByteArray data) override;
	void deleteResource(QString path) override;

private slots:

	QByteArray GET(QString token, QUrl url);
	void PUT(QString token, QUrl url, QByteArray data);
	void DELETE(QString token, QUrl url);

private:
	QString tokenOAuth;

	bool m_error = false;
	QString m_httpCode = "";
	QMap<QByteArray, QByteArray> m_httpHeaders;

	QMap<QString, QVariant> userInfo;
	QPixmap userAvatar;
	long long totalSpace = 0;
	long long usedSpace = 0;

	QMap<QString, QVariant> fileInfo;

	//QTextEdit* textEdit = new QTextEdit();
};
