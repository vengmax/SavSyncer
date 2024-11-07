#include <QtCore/QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>

QByteArray serializeFolder(const QString& folderPath) {
    QJsonObject folderObject;
    QDir dir(folderPath);
    foreach(const QString & fileName, dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QString filePath = dir.absoluteFilePath(fileName);
        if (QFileInfo(filePath).isFile()) {
            QJsonObject fileObject;
            fileObject["size"] = QFileInfo(filePath).size();
            QFile file(filePath);
            QByteArray fileData;
            if (file.open(QIODevice::ReadOnly)) {
                fileData = file.readAll();
                file.close();
            }
            fileObject["data"] = QString(fileData.toBase64());
            folderObject["file:" + fileName] = fileObject;
        }
        else {
            QJsonDocument subfolderObject = QJsonDocument::fromJson(serializeFolder(filePath));
            folderObject["folder:" + fileName] = subfolderObject.object();
        }
    }
    QJsonDocument document(folderObject);
    return document.toJson();
}

void deserializeFolder(const QByteArray& jsonData, const QString& targetPath) {
    QJsonDocument documentJSON = QJsonDocument::fromJson(jsonData);
    QJsonObject objectJSON = documentJSON.object();
    if (documentJSON.isNull() || !documentJSON.isObject()) {
        qDebug() << "Error: Data is not a JSON file";
        return;
    }

    QDir targetDir;
    targetDir.mkpath(targetPath);
    if (!targetDir.exists()) {
        qDebug() << "Error: Failed to create folder";
        return;
    }

    for (QString& key : objectJSON.keys()) {
        if (key.startsWith("folder:")) {
            QByteArray data = QJsonDocument(objectJSON.value(key).toObject()).toJson();
            deserializeFolder(data, targetPath + "/" + key.remove("folder:"));
        }
        else if (key.startsWith("file:")) {
            QJsonObject fileObject = objectJSON.value(key).toObject();
            if (fileObject.contains("data") && fileObject.contains("size")) {

                QByteArray base64Data = fileObject["data"].toString().toUtf8();
                QByteArray data = QByteArray::fromBase64(base64Data);

                QFile file(targetPath + "/" + key.remove("file:"));
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(data);
                    file.close();
                }
                else {
                    qDebug() << "Error: Cannot open file for writing";
                    return;
                }
            }
            else {
                qDebug() << "Error: Data is not a JSON file";
                return;
            }
        }
        else {
            qDebug() << "Error: Data is not a JSON file";
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

            deserializeFolder(data, distDir);

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
            data = serializeFolder(srcDir);
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
