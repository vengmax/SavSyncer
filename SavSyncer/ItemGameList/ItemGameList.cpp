#include "ItemGameList.h"

ItemGameList::ItemGameList(QSettings* regset, QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	regSettings = regset;

	connect(ui.btnIcon, SIGNAL(clicked()), this, SIGNAL(clickedMouseLeftButton()));
	connect(ui.btnSyncIcon, SIGNAL(clicked()), this, SIGNAL(clickedMouseLeftButton()));

	timerSync.setInterval(250);
	connect(&timerSync, &QTimer::timeout, this, [this]() {
		if (syncTimerCountPoint > 3)
			syncTimerCountPoint = 0;
		if(currentMode == SyncMode::SyncProcess)
			ui.labelSync->setText("Синхронизация" + QString(syncTimerCountPoint, '.') + QString(3-syncTimerCountPoint, ' '));
		else if (currentMode == SyncMode::SyncDeleteProcess)
			ui.labelSync->setText("Удаление" + QString(syncTimerCountPoint, '.') + QString(3 - syncTimerCountPoint, ' '));
		syncTimerCountPoint++;
		});
}

ItemGameList::~ItemGameList()
{
}

bool ItemGameList::isValid() {
	return valid;
}

void ItemGameList::setIcon(QIcon icon) {
	ui.btnIcon->setIcon(icon);
}
void ItemGameList::setName(QString name) {
	ui.labelName->setText(name);
}
void ItemGameList::setVersion(QString text) {
	ui.labelVersion->setText(text);
}
void ItemGameList::setSyncMode(SyncMode syncMode) {
	currentMode = syncMode;
	timerSync.stop();
	switch (syncMode) {
	case (SyncMode::SyncUncompleted):
		ui.labelSync->setText("Не синхронизировано");
		ui.labelSync->setStyleSheet("#labelSync{color: crimson;}");
		ui.btnSyncIcon->setIcon(QIcon(":/syncFailed.png"));
		break;
	case (SyncMode::SyncCompleted):
		ui.labelSync->setText("Синхронизировано");
		ui.labelSync->setStyleSheet("#labelSync{color: lightgreen;}");
		ui.btnSyncIcon->setIcon(QIcon(":/syncCompleted.png"));
		break;
	case SyncMode::SyncProcess:
		timerSync.start();
		ui.labelSync->setStyleSheet("#labelSync{color: #68B4FF;}");
		ui.btnSyncIcon->setIcon(QIcon(":/syncProcess.png"));
		break;
	case SyncMode::SyncFailed:
		ui.labelSync->setText("Ошибка");
		ui.labelSync->setStyleSheet("#labelSync{color: crimson;}");
		ui.btnSyncIcon->setIcon(QIcon(":/syncFailed.png"));
		break;
	case SyncMode::SyncDisabled:
		ui.labelSync->setText("Отключено");
		ui.labelSync->setStyleSheet("#labelSync{color: darkgray;}");
		ui.btnSyncIcon->setIcon(QIcon(":/syncFailed.png"));
		break;
	case SyncMode::SyncDeleteProcess:
		timerSync.start();
		ui.labelSync->setStyleSheet("#labelSync{color: #68B4FF;}");
		ui.btnSyncIcon->setIcon(QIcon(":/bin.png"));
		break;
	case SyncMode::SyncDeleteFailed:
		ui.labelSync->setText("Ошибка удаления");
		ui.labelSync->setStyleSheet("#labelSync{color: crimson;}");
		ui.btnSyncIcon->setIcon(QIcon(":/syncFailed.png"));
		break;
	default:
		break;
	}
}

QIcon ItemGameList::icon() {
	return ui.btnIcon->icon();
}
QString ItemGameList::name() {
	return ui.labelName->text();
}
QString ItemGameList::version() {
	return ui.labelVersion->text();
}
SyncMode ItemGameList::syncMode() {
	return currentMode;
}

void ItemGameList::setValid(bool value) {
	valid = value;
	if (!value)
		styleSheetValid = "border: 1px solid crimson;";
	else
		styleSheetValid = "";
	ui.frameBackground->setStyleSheet("#frameBackground{" + styleSheetValid + styleSheetSelected + "}");
}

void ItemGameList::setSelected(bool value) {
	selected = value;
	if(value)
		styleSheetSelected = "background-color:rgb(100,100,100);";
	else
		styleSheetSelected = "";
	ui.frameBackground->setStyleSheet("#frameBackground{" + styleSheetValid + styleSheetSelected + "}");
}

void ItemGameList::mousePressEvent(QMouseEvent* event) {
	styleSheetSelected = "background-color:rgb(100,100,100);";
	if (event->button() == Qt::LeftButton)
		ui.frameBackground->setStyleSheet("#frameBackground{" + styleSheetValid + styleSheetSelected + "}");

	QWidget::mousePressEvent(event);
}

void ItemGameList::mouseReleaseEvent(QMouseEvent* event) {
	if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
		QPoint cursorPoint = cursor().pos();
		QPoint widgetPointStart = ui.frameBackground->mapToGlobal(ui.frameBackground->geometry().topLeft());
		QPoint widgetPointEnd = ui.frameBackground->mapToGlobal(ui.frameBackground->geometry().bottomRight());
		if(cursorPoint.x() > widgetPointStart.x() - 4 && cursorPoint.x() < widgetPointEnd.x() &&
			cursorPoint.y() > widgetPointStart.y() - 4 && cursorPoint.y() < widgetPointEnd.y()) {
			if(event->button() == Qt::LeftButton)
				emit clickedMouseLeftButton();
			else
				emit clickedMouseRightButton();
		}
		else {
			if (!selected) {
				styleSheetSelected = "";
				ui.frameBackground->setStyleSheet("#frameBackground{" + styleSheetValid + styleSheetSelected + "}");
			}
		}
	}
	QWidget::mouseReleaseEvent(event);
}