#include "SavSyncer.h"
#include <QtWidgets/QApplication>
#include <csignal>
#include <dbghelp.h>
#include <QSharedMemory>

// global vars
QThread* anotherThread = new QThread();
QObject* objectAnotherThread = new QObject();
QFile *logFile = new QFile();

// crash handler
void signalHandler(int signal) {

    void* stack[100];
    unsigned short frames;
    SYMBOL_INFO* symbol;
    HANDLE process = GetCurrentProcess();

    SymInitialize(process, NULL, TRUE);
    frames = CaptureStackBackTrace(0, 100, stack, NULL);

    symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;

    QDir dir;
    dir.mkdir("crashes");
    QFile crashReport("crashes/crash_report_" + QDateTime::currentDateTime().toString("dd_MM_yyyy_hh-mm-ss") + ".txt");
    if (crashReport.open(QIODevice::WriteOnly)) {
        QString errorString = "Received signal " + QString::number(signal) + "\n";
        for (unsigned short i = 0; i < frames; i++) {
            SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);
            errorString += "Function: " + QString(symbol->Name) + "() at address: 0x" + QString::number(symbol->Address, 16) + "\n";
        }
        crashReport.write(errorString.toUtf8());
        crashReport.close();
    }

    free(symbol);
    exit(signal);
}

// message handler
void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    auto funcLogger = [type, &context, msg, dateTime = QDateTime::currentDateTime()]() {
        if (logFile->isOpen()) {
            QString prefix;
            switch (type) {
            case QtDebugMsg:
                prefix += "DEBUG: ";
                break;
            case QtWarningMsg:
                prefix += "WARNING: ";
                break;
            case QtCriticalMsg:
                prefix += "CRITICAL: ";
                break;
            case QtFatalMsg:
                prefix += "FATAL: ";
                abort();
                break;
            case QtInfoMsg:
                prefix += "INFO: ";
                break;
            }

            QString logMessage = QString(dateTime.toString("[dd.MM.yyyy hh:mm:ss.zzz] ") + prefix + msg + "\n");
            fprintf(stderr, logMessage.toLocal8Bit().constData());
            logFile->write(logMessage.toUtf8());
            logFile->flush();
        }
        };

    if (objectAnotherThread)
        QMetaObject::invokeMethod(objectAnotherThread, funcLogger, Qt::QueuedConnection);
}

// main
int main(int argc, char *argv[])
{

    // app flags
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // crash signals
    std::signal(SIGINT, signalHandler);
    std::signal(SIGILL, signalHandler);
    std::signal(SIGFPE, signalHandler);
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGBREAK, signalHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGABRT_COMPAT, signalHandler);

    // locale
    setlocale(LC_ALL, "Russian");
    QLocale::setDefault(QLocale(QLocale::Russian));

    // app
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon(QPixmap(":/logo.png")));

    // program duplication blocking
    QSharedMemory sharedMemory("SavSyncer");
    if (sharedMemory.attach()) {
        QMessageBox::critical(nullptr, "Ошибка", "Программа уже запущена!");
        return 0;
    }
    sharedMemory.create(1);

    // logs
    objectAnotherThread->moveToThread(anotherThread);
    QObject::connect(&a, &QApplication::destroyed, anotherThread, &QThread::quit);
    QObject::connect(anotherThread, &QThread::finished, anotherThread, &QThread::deleteLater);
    QObject::connect(&a, &QApplication::destroyed, objectAnotherThread, &QObject::deleteLater);
    QObject::connect(&a, &QApplication::destroyed, logFile, &QObject::deleteLater);
    anotherThread->start();

    // timer abort
    QTimer timerQuit;
    timerQuit.setSingleShot(true);
    timerQuit.setInterval(10000);
    QObject::connect(&timerQuit, &QTimer::timeout, &a, [&a]() { abort(); });

    // check thread
    timerQuit.start();
    if (!anotherThread->isRunning())
        QCoreApplication::processEvents();
    timerQuit.stop();

    // check dir
    QDir dirLog(a.applicationDirPath());
    timerQuit.start();
    while (dirLog.mkdir("logs"))
        QCoreApplication::processEvents();
    timerQuit.stop();

    // create logs file
    QString date = QDateTime::currentDateTime().toString("dd_MM_yyyy_hh-mm-ss");
    logFile->setFileName(a.applicationDirPath() + "/logs/" + date + ".log");
    logFile->open(QIODevice::WriteOnly);
    timerQuit.start();
    while (!logFile->isOpen()) {
        logFile->open(QIODevice::WriteOnly);
        QCoreApplication::processEvents();
    }
    timerQuit.stop();
    qInstallMessageHandler(messageHandler);

    // main widget
    SavSyncer w;

    // backup
    QObject::connect(&a, &QCoreApplication::aboutToQuit, &w, &SavSyncer::onAboutToQuit);

    // start app minimized
    QStringList args = a.arguments();
    if (!args.contains("--hide"))
        w.show();
    
    return a.exec();
}
