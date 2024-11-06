#include "YandexService.h"

YandexService::YandexService() {

}

YandexService::~YandexService() {

}

QString YandexService::name() {
    return "Yandex";
}

QString YandexService::widgetText() {
    return "Войти через Яндекс";
}
QColor YandexService::widgetTextColor() {
    return QColor("white");
}
QColor YandexService::widgetBackgroundColor() {
    return QColor("black");
}
QIcon YandexService::widgetIcon() {
    return QIcon(":/YandexIcons/yandexLogo.png");
}

bool YandexService::error() {
    return m_error;
}

QString YandexService::httpCode() {
    return m_httpCode;
}

QMap<QByteArray, QByteArray> YandexService::httpHeaders() {
    return m_httpHeaders;
}

QString YandexService::authorization() {
    m_error = false;

    INFO_MSG("Start authorization");
    QDialog* dialog = new QDialog();
    dialog->setWindowFlags(dialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);
    QWebEngineView* webView = new QWebEngineView();
    QVBoxLayout* layoutDialog = new QVBoxLayout();
    layoutDialog->setContentsMargins(0, 0, 0, 0);
    layoutDialog->setSpacing(0);
    layoutDialog->setMargin(0);
    layoutDialog->addWidget(webView);
    dialog->setLayout(layoutDialog);

    QString accessToken;
    connect(webView, &QWebEngineView::urlChanged, this, [dialog, &accessToken](const QUrl& url) {
        if (url.toString().startsWith(REDIRECT_URI)) {
            QUrlQuery query(url.fragment());
            accessToken = query.queryItemValue("access_token");
            dialog->accept();
        }
        });

    QString url = QString("https://oauth.yandex.ru/authorize?response_type=token&client_id=%1&redirect_uri=%2").arg(CLIENT_ID, REDIRECT_URI);
    webView->load(QUrl(url));
    webView->show();
    if (dialog->exec() != QDialog::Accepted) {
        accessToken = "";
        INFO_MSG("Authorization failed");
        m_error = true;
    }
    else
        INFO_MSG("Authorization successful!");
    dialog->deleteLater();
    return accessToken;
}

void YandexService::signIn(QString token) {
    m_error = false;

    tokenOAuth = token;
    INFO_MSG("Start sign in");
    loadUserInfo();
    if (!m_error)
        INFO_MSG("Successful sign in");
    else
        CRITICAL_MSG("Sign in failed");
}

void YandexService::signOut() {
    m_error = false;

    INFO_MSG("Start sign out");
    tokenOAuth = "";
    QWebEngineProfile* profile = QWebEngineProfile::defaultProfile();
    profile->cookieStore()->deleteAllCookies();
    INFO_MSG("Successful sign out");
}

void YandexService::loadUserInfo() {
    m_error = false;

    INFO_MSG("Loading user data");
    QByteArray response = GET(tokenOAuth, QUrl("https://login.yandex.ru/info"));
    if (m_error)
        return;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        CRITICAL_MSG("Response is not a JSON file");
        m_error = true;
        return;
    }

    userInfo.clear();
    QJsonObject jsonObj = jsonDoc.object();
    for (const QString& key : jsonObj.keys()) {
        QJsonValue value = jsonObj.value(key);
        userInfo.insert(key, value.toVariant());
    }
    INFO_MSG("User data loading completed");

    // avatar
    INFO_MSG("Load user avatar");
    if (userInfo.contains("default_avatar_id")) {
        response = GET(tokenOAuth, QUrl(QString("https://avatars.yandex.net/get-yapic/%1/islands-200").arg(userInfo["default_avatar_id"].toString())));
        if (m_error) {
            WARNING_MSG("User avatar loading failed");
            m_error = false;
        } 
        else {
            userAvatar.loadFromData(response, "JPEG");
            INFO_MSG("User avatar loading completed");
        }
    }
    else {
        WARNING_MSG("Key \"default_avatar_id\" not found");
    }

    // storage space
    INFO_MSG("Load storage info");
    response = GET(tokenOAuth, QUrl("https://cloud-api.yandex.net/v1/disk/"));
    if (m_error)
        return;

    jsonDoc = QJsonDocument::fromJson(response);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        CRITICAL_MSG("Response is not a JSON file");
        m_error = true;
        return;
    }

    jsonObj = jsonDoc.object();
    if (jsonObj.value("total_space").isDouble()) {
        totalSpace = jsonObj.value("total_space").toDouble();
    }
    else {
        CRITICAL_MSG("Key \"total_space\" not found");
        m_error = true;
        return;
    }
    if (jsonObj.value("used_space").isDouble()) {
        usedSpace = jsonObj.value("used_space").toDouble();
    }
    else {
        CRITICAL_MSG("Key \"used_space\" not found");
        m_error = true;
        return;
    }
    INFO_MSG("Storage info loading completed");
    return;
}

QString YandexService::userId() {
    return userInfo.value("id", "").toString();
}
QString YandexService::login() {
    return userInfo.value("login", "").toString();
}
QString YandexService::firstName() {
    return userInfo.value("first_name", "").toString();
}
QString YandexService::lastName() {
    return userInfo.value("last_name", "").toString();
}

QPixmap YandexService::avatar() {
    return userAvatar;
}
long long YandexService::storageTotalSpace() {
    return totalSpace;
}
long long YandexService::storageUsedSpace() {
    return usedSpace;
}

bool YandexService::checkFileExistence(QString path) {
    m_error = false;

    INFO_MSG("Start check file existence (path: " + path + ")");

    QByteArray response = GET(tokenOAuth, QUrl("https://cloud-api.yandex.net/v1/disk/resources?path=app:/&fields=_embedded.items"));
    if (m_error)
        return false;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
    QJsonObject jsonObj = jsonDoc.object();

    // Проверяем, есть ли вложенные элементы
    if (jsonObj.contains("_embedded")) {
        QJsonObject embedded = jsonObj["_embedded"].toObject();
        QJsonArray items = embedded["items"].toArray();

        // Перебираем папки и выводим их имена
        for (const QJsonValue& item : items) {
            QJsonObject folderObj = item.toObject();
            QString fileName = folderObj["name"].toString();
            if (fileName == path) {
                INFO_MSG("File found");
                return true;
            }
        }
    }
    INFO_MSG("File not found");
    return false;
}

QStringList YandexService::listFileNames(QString path) {
    m_error = false;

    INFO_MSG("Start load list file names (path: " + path + ")");

    QByteArray response = GET(tokenOAuth, QUrl("https://cloud-api.yandex.net/v1/disk/resources?path=app:/" + path + "&fields=_embedded.items"));
    if (m_error)
        return QStringList();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
    QJsonObject jsonObj = jsonDoc.object();

    if (jsonObj.contains("_embedded")) {
        QJsonObject embedded = jsonObj["_embedded"].toObject();
        QJsonArray items = embedded["items"].toArray();

        QStringList output;
        for (const QJsonValue& item : items) {
            QJsonObject folderObj = item.toObject();
            QString fileName = folderObj["name"].toString();
            output.append(fileName);
        }
        INFO_MSG("Files found");
        return output;
    }
    INFO_MSG("Files not found");
    return QStringList();
}

void YandexService::loadFileInfo(QString path) {
    m_error = false;

    INFO_MSG("Start load file data (path: " + path + ")");

    QByteArray response = GET(tokenOAuth, QUrl("https://cloud-api.yandex.net/v1/disk/resources?path=app:/" + path));
    if (m_error)
        return;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        CRITICAL_MSG("Response is not a JSON file");
        m_error = true;
        return;
    }

    fileInfo.clear();
    QJsonObject jsonObj = jsonDoc.object();
    for (const QString& key : jsonObj.keys()) {
        QJsonValue value = jsonObj.value(key);
        fileInfo.insert(key, value.toVariant());
    }

    INFO_MSG("File info loading completed");
}

QDateTime YandexService::lastModified() {
    m_error = false;

    if (fileInfo.contains("modified")) {
        QStringList dateAndTimeZone = fileInfo.value("modified").toString().split("+");
        QDateTime date;
        if (dateAndTimeZone.size() == 2) {
            date = QDateTime::fromString(dateAndTimeZone.at(0), "yyyy-MM-ddThh:mm:ss");
            return date;
        }
        else {
            CRITICAL_MSG("Failed to convert date and time");
            m_error = true;
            return QDateTime();
        }
    }
    return QDateTime();
}

QByteArray YandexService::download(QString path) {
    m_error = false;

    INFO_MSG("Start download data (path: " + path + ")");

    QByteArray response = GET(tokenOAuth, QUrl("https://cloud-api.yandex.net/v1/disk/resources/download?path=app:/" + path));
    if (m_error)
        return QByteArray();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        CRITICAL_MSG("Response is not a JSON file");
        m_error = true;
        return QByteArray();
    }

    QString pathDownload;
    QJsonObject jsonObj = jsonDoc.object();
    if (jsonObj.value("href").isString()) {
        pathDownload = jsonObj.value("href").toString();
        if (pathDownload.isEmpty()) {
            CRITICAL_MSG("The path for downloading data is empty");
            m_error = true;
            return QByteArray();
        }
    }
    else {
        CRITICAL_MSG("Key \"href\" not found");
        m_error = true;
        return QByteArray();
    }

    if (jsonObj.value("method").isString()) {
        if (jsonObj.value("method").toString() != "GET") {
            CRITICAL_MSG("The data loading method is incorrect");
            m_error = true;
            return QByteArray();
        }
    }
    else {
        CRITICAL_MSG("Key \"method\" not found");
        m_error = true;
        return QByteArray();
    }

    if (jsonObj.value("templated").isBool()) {
        if (jsonObj.value("templated").toBool()) {
            CRITICAL_MSG("Download link is templated");
            m_error = true;
            return QByteArray();
        }
    }
    else {
        CRITICAL_MSG("Key \"templated\" not found");
        m_error = true;
        return QByteArray();
    }

    response = GET(tokenOAuth, QUrl(QString(pathDownload)));
    if (m_error)
        return QByteArray();

    if (m_httpCode != "200")
        return QByteArray();

    INFO_MSG("Data downloaded");

    return response;
}

void YandexService::upload(QString path, QByteArray data) {
    m_error = false;

    INFO_MSG("Start upload data (path: " + path + ")");

    QByteArray response = GET(tokenOAuth, QUrl("https://cloud-api.yandex.net/v1/disk/resources/upload?path=app:/" + path + "&overwrite=true"));
    if (m_error)
        return;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        CRITICAL_MSG("Response is not a JSON file");
        m_error = true;
        return;
    }

    QString pathUpload;
    QJsonObject jsonObj = jsonDoc.object();
    if (jsonObj.value("href").isString()) {
        pathUpload = jsonObj.value("href").toString();
        if (pathUpload.isEmpty()) {
            CRITICAL_MSG("The path for downloading data is empty");
            m_error = true;
            return;
        }
    }
    else {
        CRITICAL_MSG("Key \"href\" not found");
        m_error = true;
        return;
    }

    if (jsonObj.value("method").isString()) {
        if (jsonObj.value("method").toString() != "PUT") {
            CRITICAL_MSG("The data loading method is incorrect");
            m_error = true;
            return;
        }
    }
    else {
        CRITICAL_MSG("Key \"method\" not found");
        m_error = true;
        return;
    }

    if (jsonObj.value("templated").isBool()) {
        if (jsonObj.value("templated").toBool()) {
            CRITICAL_MSG("Download link is templated");
            m_error = true;
            return;
        }
    }
    else {
        CRITICAL_MSG("Key \"templated\" not found");
        m_error = true;
        return;
    }

    PUT(tokenOAuth, QUrl(QString(pathUpload)), data);
    if (m_httpCode != "201") {
        if (m_httpCode == "202") 
            WARNING_MSG("The file is being processed");
        else if (m_httpCode == "413")
            CRITICAL_MSG("File size is larger than allowed! Allowed size: up to 1 GB");
        else if (m_httpCode == "500" || m_httpCode == "503")
            CRITICAL_MSG("Server error, please try downloading again");
        else if (m_httpCode == "507")
            CRITICAL_MSG("There is not enough space on the user's Disk to download the file");
        else
            CRITICAL_MSG("Failed to upload data");
    }
    INFO_MSG("Data uploaded");
}

void YandexService::deleteResource(QString path) {
    m_error = false;

    INFO_MSG("Start delete data (path: " + path + ")");

    DELETE(tokenOAuth, QUrl("https://cloud-api.yandex.net/v1/disk/resources?path=app:/" + path + "&permanently=true"));
    if (m_error)
        return;

    INFO_MSG("Data deleted");
}

QByteArray YandexService::GET(QString token, QUrl url) {
    m_error = false;

    QByteArray response;
    if (!token.isEmpty()) {
        QNetworkAccessManager* manager = new QNetworkAccessManager();
        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QString("OAuth %1").arg(token).toUtf8());

        QEventLoop eventLoop;
        QNetworkReply* reply = manager->get(request);
        connect(reply, &QNetworkReply::finished, [this, reply, manager, &eventLoop, &response, &token]() {

            //QString attributeString;
            m_httpCode = reply->attribute(QNetworkRequest::Attribute::HttpStatusCodeAttribute).toString();
            //attributeString += httpCode + "\r\n";
            for (auto item : reply->rawHeaderList()) {
                m_httpHeaders.insert(item, reply->rawHeader(item));
                //attributeString += item + " " + reply->rawHeader(item) + "\r\n";
            }

            if (reply->error() == QNetworkReply::NoError) {
                if (m_httpCode.toInt() > 299 && m_httpCode.toInt() < 400) {
                    INFO_MSG("Redirect to another address");
                    response = GET(token, QUrl(reply->rawHeader("Location")));
                }
                else {
                    response = reply->readAll();
                }
                /*textEdit->setText(textEdit->toPlainText() + attributeString + "\r\n\r\n" + response);
                textEdit->show();*/
            }
            else {
                m_error = true;
                CRITICAL_MSG("Error during GET request: unparsed code " + m_httpCode);
                /*textEdit->setText(textEdit->toPlainText() + attributeString + "\r\n\r\n" + reply->errorString());
                textEdit->show();*/
            }

            eventLoop.quit();
            });

        eventLoop.exec();
        return response;
    }
    else {
        CRITICAL_MSG("Token is empty!");
        m_error = true;
        return response;
    }
}

void YandexService::PUT(QString token, QUrl url, QByteArray data) {
    m_error = false;

    QByteArray response;
    if (!token.isEmpty()) {
        QNetworkAccessManager* manager = new QNetworkAccessManager();
        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QString("OAuth %1").arg(token).toUtf8());

        QEventLoop eventLoop;
        QNetworkReply* reply = manager->put(request, data);
        connect(reply, &QNetworkReply::finished, [this, reply, manager, &eventLoop, &response, &token, &data]() {

            //QString attributeString;
            m_httpCode = reply->attribute(QNetworkRequest::Attribute::HttpStatusCodeAttribute).toString();
            //attributeString += httpCode + "\r\n";
            for (auto item : reply->rawHeaderList()) {
                m_httpHeaders.insert(item, reply->rawHeader(item));
                //attributeString += item + " " + reply->rawHeader(item) + "\r\n";
            }

            if (reply->error() == QNetworkReply::NoError) {
                if (m_httpCode.toInt() > 299 && m_httpCode.toInt() < 400) {
                    INFO_MSG("Redirect to another address");
                    PUT(token, QUrl(reply->rawHeader("Location")), data);
                }
                else {
                    response = reply->readAll();
                }
                /*textEdit->setText(textEdit->toPlainText() + attributeString + "\r\n\r\n" + response);
                textEdit->show();*/
            }
            else {
                m_error = true;
                CRITICAL_MSG("Error during PUT request: unparsed code " + m_httpCode);
                /*textEdit->setText(textEdit->toPlainText() + attributeString + "\r\n\r\n" + reply->errorString());
                textEdit->show();*/
            }

            eventLoop.quit();
            });

        eventLoop.exec();
    }
    else {
        CRITICAL_MSG("Token is empty!");
        m_error = true;
    }
}

void YandexService::DELETE(QString token, QUrl url) {
    m_error = false;

    QByteArray response;
    if (!token.isEmpty()) {
        QNetworkAccessManager* manager = new QNetworkAccessManager();
        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QString("OAuth %1").arg(token).toUtf8());

        QEventLoop eventLoop;
        QNetworkReply* reply = manager->deleteResource(request);
        connect(reply, &QNetworkReply::finished, [this, reply, manager, &eventLoop, &response, &token]() {

            //QString attributeString;
            m_httpCode = reply->attribute(QNetworkRequest::Attribute::HttpStatusCodeAttribute).toString();
            //attributeString += httpCode + "\r\n";
            for (auto item : reply->rawHeaderList()) {
                m_httpHeaders.insert(item, reply->rawHeader(item));
                //attributeString += item + " " + reply->rawHeader(item) + "\r\n";
            }

            if (reply->error() == QNetworkReply::NoError) {
                /*if (m_httpCode.toInt() > 299 && m_httpCode.toInt() < 400) {
                    INFO_MSG("Redirect to another address");
                    response = GET(token, QUrl(reply->rawHeader("Location")));
                }
                else {*/
                    response = reply->readAll();
                //}
                /*textEdit->setText(textEdit->toPlainText() + attributeString + "\r\n\r\n" + response);
                textEdit->show();*/
            }
            else {
                m_error = true;
                CRITICAL_MSG("Error during DELETE request: unparsed code " + m_httpCode);
                /*textEdit->setText(textEdit->toPlainText() + attributeString + "\r\n\r\n" + reply->errorString());
                textEdit->show();*/
            }

            eventLoop.quit();
            });

        eventLoop.exec();
        //return response;
    }
    else {
        CRITICAL_MSG("Token is empty!");
        m_error = true;
        //return response;
    }
}