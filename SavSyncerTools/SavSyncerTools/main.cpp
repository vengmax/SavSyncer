#include <QtCore/QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <Windows.h>

QByteArray serializeFolder(const QString& folderPath, qint64& startSize, bool& ok) {
    ok = true;

    QJsonObject folderObject;
    QDir dir(folderPath);
    foreach(const QString & fileName, dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QString filePath = dir.absoluteFilePath(fileName);
        if (QFileInfo(filePath).isFile()) {
            QJsonObject fileObject;

            // check size file
            qint64 sizeFileBase64 = ceil(QFileInfo(filePath).size() / (double)3) * 4;
            if ((startSize + sizeFileBase64) >= (qint64)(1.4 * 1024 * 1024 * 1024 - 1024)) {
                qDebug() << "Error: Maximum game data size exceeded (max size: 1 GB)";
                ok = false;
                return QByteArray();
            }

            // load file
            QFile file(filePath);
            QByteArray fileData;
            if (file.open(QIODevice::ReadOnly)) {
                fileData = file.readAll();
                file.close();
            }
            else {
                qDebug() << "Error: Failed to open file " + filePath;
                ok = false;
                return QByteArray();
            }

            // create json
            fileObject["size"] = QFileInfo(filePath).size();
            fileObject["modified"] = QFileInfo(filePath).lastModified().toString("yyyy_MM_dd_hh-mm-ss-zzz");
            fileObject["data"] = ".";

            // check size file + json
            qint64 sizeJsonFile = startSize + QJsonDocument(fileObject).toJson(QJsonDocument::Compact).size() + sizeFileBase64;
            if (sizeJsonFile >= (qint64)(1.4 * 1024 * 1024 * 1024 - 1024)) {
                qDebug() << "Error: Maximum game data size exceeded (max size: 1 GB)";
                ok = false;
                return QByteArray();
            }

            fileObject["data"] = QString(fileData.toBase64());
            folderObject["file:" + fileName] = fileObject;

            int serviceSymbols = 6;
            startSize += QJsonDocument(fileObject).toJson(QJsonDocument::Compact).size() + QString("file:" + fileName).size() + serviceSymbols;
        }
        else {
            QByteArray dataJSON = serializeFolder(filePath, startSize, ok);
            if (!ok)
                return QByteArray();

            QJsonDocument subfolderObject = QJsonDocument::fromJson(dataJSON);
            folderObject["folder:" + fileName] = subfolderObject.object();

            int serviceSymbols = 6;
            startSize += QString("folder:" + fileName).size() + serviceSymbols;
        }
    }
    QJsonDocument document(folderObject);
    return document.toJson(QJsonDocument::Compact);
}

bool setLastModifiedTime(const QString& filePath, const QDateTime& newTime) {
    HANDLE hFile = CreateFileA(
        filePath.toStdString().c_str(),
        GENERIC_WRITE,
        FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        CloseHandle(hFile);
        return false;
    }

    ULARGE_INTEGER ull;
    ull.QuadPart = newTime.toMSecsSinceEpoch() * 10000 + 116444736000000000;

    FILETIME ft;
    ft.dwLowDateTime = ull.LowPart;
    ft.dwHighDateTime = ull.HighPart;

    // Установка времени последнего изменения
    if (!SetFileTime(hFile, NULL, NULL, &ft)) {
        CloseHandle(hFile);
        return false;
    }

    CloseHandle(hFile);
    return true;
}

void deserializeFolder(const QByteArray& jsonData, const QString& targetPath, bool& ok) {
    ok = true;

    QJsonDocument documentJSON = QJsonDocument::fromJson(jsonData);
    QJsonObject objectJSON = documentJSON.object();
    if (documentJSON.isNull() || !documentJSON.isObject()) {
        qDebug() << "Error: Data is not a JSON file";
        ok = false;
        return;
    }

    QDir targetDir;
    targetDir.mkpath(targetPath);
    if (!targetDir.exists()) {
        qDebug() << "Error: Failed to create folder";
        ok = false;
        return;
    }

    for (QString& key : objectJSON.keys()) {
        if (key.startsWith("folder:")) {
            QByteArray data = QJsonDocument(objectJSON.value(key).toObject()).toJson(QJsonDocument::Compact);
            deserializeFolder(data, targetPath + "/" + key.remove("folder:"), ok);
            if (!ok)
                return;
        }
        else if (key.startsWith("file:")) {
            QJsonObject fileObject = objectJSON.value(key).toObject();
            if (fileObject.contains("data") && fileObject.contains("size") && fileObject.contains("modified")) {

                QByteArray base64Data = fileObject["data"].toString().toUtf8();
                QDateTime dateModified = QDateTime::fromString(fileObject["modified"].toString(), "yyyy_MM_dd_hh-mm-ss-zzz");
                QByteArray data = QByteArray::fromBase64(base64Data);

                QFile file(targetPath + "/" + key.remove("file:"));
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(data);
                    file.close();
                    setLastModifiedTime(file.fileName(), dateModified);
                }
                else {
                    qDebug() << "Error: Cannot open file for writing";
                    ok = false;
                    return;
                }
            }
            else {
                qDebug() << "Error: Data is not a correct JSON file";
                ok = false;
                return;
            }
        }
        else {
            qDebug() << "Error: Data is not a JSON file";
            ok = false;
            return;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
        return 0;

    QCoreApplication a(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("\nTools for working with game data of the SavSincer program");
    parser.addHelpOption();

    QCommandLineOption unpackOption(QStringList() << "u" << "unpack", "Unpack game data.", "source file");
    parser.addOption(unpackOption);

    QCommandLineOption packOption(QStringList() << "p" << "pack", "Pack game data.", "source directory");
    parser.addOption(packOption);

    QCommandLineOption dirOption(QStringList() << "d" << "dir", "Destination directory for unpacking.", "destination directory");
    parser.addOption(dirOption);

    QCommandLineOption fileOption(QStringList() << "f" << "file", "Destination file for packing.", "destination file");
    parser.addOption(fileOption);

    parser.process(a);

    bool isPackOrUnpackSet = parser.isSet(unpackOption) || parser.isSet(packOption);

    if (parser.isSet(unpackOption)) {
        QString srcFile = parser.value(unpackOption);
        if (QFileInfo(srcFile).suffix() == "savsyncer") {
            QByteArray data;
            QFile file(srcFile);
            if (file.open(QIODevice::ReadOnly)) {
                data = file.readAll();
                file.close();
            }
            else {
                qDebug() << "Error: Unable to open file.";
                return 1;
            }
            qDebug() << "Unpacking in progress...";
            data = qUncompress(data);
            QString distDir = QCoreApplication::applicationDirPath();
            if (parser.isSet(dirOption))
                distDir = parser.value(dirOption);

            bool ok = true;
            deserializeFolder(data, distDir, ok);
            if (!ok) {
                qDebug() << "Error: Data deserialization failed";
                return 1;
            }

            qDebug() << "Unpacking completed!";
            return 0;
        }
        else {
            qDebug() << "Error: Invalid file format. The file must have the extension .savsyncer";
            return 1;
        }
    }

    if (parser.isSet(packOption)) {
        QString srcDir = parser.value(packOption);
        if (QFileInfo(srcDir).isDir()) {
            qDebug() << "Packing in progress...";
            QByteArray data;
            
            bool ok = true;
            qint64 startSize = 0;
            data = serializeFolder(srcDir, startSize, ok);
            if (!ok) {
                qDebug() << "Error: Data serialization failed";
                return 1;
            }

            data = qCompress(data, 4);
            
            int number = 1;
            QString distFile = QCoreApplication::applicationDirPath() + "/GameData_" + QString::number(number) + ".savsyncer";
            while (QFileInfo(distFile).exists()) {
                distFile = QCoreApplication::applicationDirPath() + "/GameData_" + QString::number(number) + ".savsyncer";
                number++;
            }

            if (parser.isSet(fileOption)) {
                distFile = parser.value(fileOption);
                QFileInfo fileInfo(distFile);
                if (fileInfo.suffix() != "savsyncer") {
                    if (fileInfo.suffix().isEmpty()) {
                        if(distFile.at(distFile.size() - 1) == '.')
                            distFile += "savsyncer";
                        else
                            distFile += ".savsyncer";
                    }
                    else {
                        distFile.remove(distFile.lastIndexOf(fileInfo.suffix()), fileInfo.suffix().size());
                        distFile += "savsyncer";
                    }
                }
            }

            QDir targetDir;
            QString pathDir = distFile;
            QString fileName = QFileInfo(distFile).fileName();
            pathDir.remove(pathDir.lastIndexOf(fileName), fileName.size());
            if (!pathDir.isEmpty()) {
                if (!QDir(pathDir).isEmpty()) {
                    targetDir.mkpath(pathDir);
                    if (!targetDir.exists()) {
                        qDebug() << "Error: Failed to create folders";
                        return 1;
                    }
                }
            }

            QFile file(distFile);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(data);
                file.close();
            }
            else {
                qDebug() << "Error: Unable to save file.";
                return 1;
            }

            qDebug() << "Packaging completed!";
            return 0;
        }
        else {
            qDebug() << "Error: Invalid directory.";
            return 1;
        }
    }

    return 0;
}
