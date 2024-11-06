#include <QObject>
#include <QDebug>

#define DEBUG_MSG(message) qDebug() << QString(__FUNCTION__) << "---" << QString((message))
#define WARNING_MSG(message) qWarning() << QString(__FUNCTION__) << "---" << QString((message))
#define CRITICAL_MSG(message) qCritical() << QString(__FUNCTION__) << "---" << QString((message))
#define FATAL_MSG(message) qFatal() << QString(__FUNCTION__) << "---" << QString((message))
#define INFO_MSG(message) qInfo() << QString(__FUNCTION__) << "---" << QString((message))