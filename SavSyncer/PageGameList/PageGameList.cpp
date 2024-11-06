#include "PageGameList.h"

PageGameList::PageGameList(QSettings* regset, QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	regSettings = regset;

	connect(ui.checkBoxSync, &QCheckBox::stateChanged, this, [this](int state) {
		if (state == 0)
			emit enabledSync(false);
		else
			emit enabledSync(true);
		});

	connect(ui.btnClearPathGame, &QPushButton::clicked, this, [this]() { 
		ui.lineEditPathGame->clear(); 
		emit updateGameSaveInfo();
		});
	connect(ui.btnClearPathGameSave, &QPushButton::clicked, this, [this]() { 
		ui.lineEditPathGameSave->clear(); 
		emit updateGameSaveInfo();
		});

	connect(ui.btnReviewGame, &QPushButton::clicked, this, [this]() {
		QString path = QFileDialog::getOpenFileName(nullptr, "Путь к игре", "", "Executable file (*.exe)");
		if (!path.isEmpty())
			ui.lineEditPathGame->setText(path);
		ui.lineEditPathGame->setCursorPosition(0);
		emit updateGameSaveInfo();
		});
	connect(ui.btnReviewGameSave, &QPushButton::clicked, this, [this]() {
		QString path = QFileDialog::getExistingDirectory(nullptr, "Путь к папке с сохранениями игры");
		if(!path.isEmpty())
			ui.lineEditPathGameSave->setText(path);
		ui.lineEditPathGameSave->setCursorPosition(0);
		emit updateGameSaveInfo();
		});

	connect(ui.btnUpdateGameSaveInfo, SIGNAL(clicked()), this, SIGNAL(updateGameSaveInfo()));

	connect(ui.lineEditPathGame, &QLineEdit::textChanged, this, [this](const QString& text) {
		QFileInfo checkInfo = QFileInfo(text);
		if (!checkInfo.exists() || !checkInfo.isFile()) {
			ui.lineEditPathGame->setStyleSheet("border: 1px solid crimson;");
			validPathGame = false;
			emit validPath(false);
		}
		else {
			ui.lineEditPathGame->setStyleSheet("");
			validPathGame = true;
			emit validPath(true);
			emit updateGameInfo();
		}
		emit updateGameSaveInfo();
		});

	connect(ui.lineEditPathGameSave, &QLineEdit::textChanged, this, [this](const QString& text) {
		QFileInfo checkInfo = QFileInfo(text);
		if (!checkInfo.exists() || !checkInfo.isDir()) {
			ui.lineEditPathGameSave->setStyleSheet("border: 1px solid crimson;");
			validPathGameSave = false;
			emit validPath(false);
		}
		else {
			ui.lineEditPathGameSave->setStyleSheet("");
			validPathGameSave = true;
			emit validPath(true);
		}
		emit updateGameSaveInfo();
		});

	ui.lineEditPathGame->textChanged("");
	ui.lineEditPathGameSave->textChanged("");
}

PageGameList::~PageGameList()
{
}

void PageGameList::setTitle(QString text) {
	ui.labelTitle->setText(text);
}

void PageGameList::setPathGame(QString path) {
	ui.lineEditPathGame->setText(path);
	ui.lineEditPathGame->setCursorPosition(0);
}
void PageGameList::setPathGameSave(QString path) {
	ui.lineEditPathGameSave->setText(path);
	ui.lineEditPathGameSave->setCursorPosition(0);
}
void PageGameList::setGameSaveSize(QString gameSaveSize) {
	ui.labelSaveSize->setText(gameSaveSize);
}
void PageGameList::setGameSaveLastModified(QString date) {
	ui.labelDateLastModified->setText(date);
}
void PageGameList::setGameSaveLastSync(QString date) {
	ui.labelDateLastSync->setText(date);
}
void PageGameList::setGameLastRun(QString date) {
	ui.labelDateLastRun->setText(date);
}

QString PageGameList::pathGame() {
	return ui.lineEditPathGame->text();
}
QString PageGameList::pathGameSave() {
	return ui.lineEditPathGameSave->text();
}
QString PageGameList::gameSaveSize() {
	return ui.labelSaveSize->text();
}
QString PageGameList::gameSaveLastModified() {
	return ui.labelDateLastModified->text();
}
QString PageGameList::gameSaveLastSync() {
	return ui.labelDateLastSync->text();
}
QString PageGameList::gameLastRun() {
	return ui.labelDateLastRun->text();
}

bool PageGameList::isSync() {
	return ui.checkBoxSync->isChecked();
}
void PageGameList::setSyncEnable(bool value) {
	ui.checkBoxSync->setChecked(value);
}

bool PageGameList::isValidPathGame() {
	return validPathGame;
}

bool PageGameList::isValidPathGameSave() {
	return validPathGameSave;
}