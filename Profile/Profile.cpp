#include "Profile.h"

Profile::Profile(QSettings* regset, QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	regSettings = regset;

	ui.btnSignOut->setVisible(false);
	ui.frameStorage->setVisible(false);
	connect(ui.btnSignOut, SIGNAL(clicked()), this, SIGNAL(signalSignOut()));

	QString lastSignIn = regSettings->value("Services/SignIn").toString();

	QStringList listNamesServiceDLL = QDir(QCoreApplication::applicationDirPath() + "/services").entryList();
	for (const QString& nameServiceDLL : listNamesServiceDLL) {
		if (nameServiceDLL.split('.').last().toUpper().contains("DLL")) {
			QPluginLoader loaderServicesDLL(QCoreApplication::applicationDirPath() + "/services/" + nameServiceDLL);
			QObject* obj = loaderServicesDLL.instance();
			IServices* service = dynamic_cast<IServices*>(loaderServicesDLL.instance());
			if (service) {
				services.append(service);

				createWidgetAuthorization(service);

				if (service->name() == lastSignIn)
					lastAuthService = service;
			}
			else {
				QMessageBox::warning(nullptr, "Сервисы", "Не удалось загрузить модуль " + nameServiceDLL + ".dll");
			}
		}
	}
}

Profile::~Profile() {
	qDeleteAll(services);
	services.clear();
}

QString Profile::serviceName() {
	if (lastAuthService)
		return lastAuthService->name();
	return "Unknown";
}

bool Profile::error() {
	if (lastAuthService)
		return lastAuthService->error();
	return true;
}
bool Profile::isOnline() {
	return flagOnline;
}

void Profile::signIn() {

	if (lastAuthService) {
		INFO_MSG(QString("Sign in to " + lastAuthService->name()));
		QString token = regSettings->value("Services/" + lastAuthService->name() + "/token").toString();

		lastAuthService->signIn(token);
		if (!lastAuthService->error()) {
			INFO_MSG("Sign in to " + lastAuthService->name() + " successfully");

			showProfile(Status::Online);

			regSettings->setValue("Services/SignIn", lastAuthService->name());
			flagOnline = true;
			emit signalSuccessfulSignIn();
		}
		else {
			INFO_MSG("Sign in to " + lastAuthService->name() + " failed");
			lastAuthService = nullptr;
			regSettings->setValue("Services/SignIn", "");
			flagOnline = false;
			emit signalFailedSignIn();
		}
	}
}

void Profile::signOut() {
	regSettings->setValue("Services/SignIn", "");

	if(lastAuthService)
		lastAuthService->signOut();
	lastAuthService = nullptr;
	showProfile(Status::Unknown);

	flagOnline = false;
}

void Profile::loadUserData() {
	if (lastAuthService) {
		lastAuthService->loadUserInfo();
		if (!lastAuthService->error()) 
			showProfile(Status::Online);
		else
			showProfile(Status::Offline);
	}
}

QString Profile::getId() {
	if (lastAuthService)
		return lastAuthService->userId();
	return QString();
}

QString Profile::getFirstName() {
	if (lastAuthService)
		return lastAuthService->firstName();
	return QString();
}

QString Profile::getLastName() {
	if (lastAuthService)
		return lastAuthService->lastName();
	return QString();
}

QPixmap Profile::getAvatar() {
	if (lastAuthService)
		return lastAuthService->avatar();
	return QString();
}

bool Profile::checkFileExistence(QString path, QString nameFile) {
	if (lastAuthService)
		return lastAuthService->checkFileExistence(path, nameFile);
	return false;
}

QStringList Profile::listFiles(QString path) {
	if (lastAuthService)
		return lastAuthService->listFileNames(path);
	return QStringList();
}

void Profile::loadGameDataInfo(QString path) {
	if (lastAuthService)
		lastAuthService->loadFileInfo(path);
}

QDateTime Profile::lastModifiedGameData() {
	if (lastAuthService)
		return lastAuthService->lastModified();
	return QDateTime();
}

void Profile::uploadGameData(QString path, QByteArray compressed) {
	if (lastAuthService)
		lastAuthService->upload(path, compressed);
}

QByteArray Profile::downloadGameData(QString path) {
	if (lastAuthService)
		return lastAuthService->download(path);
	return QByteArray();
}

void Profile::deleteGameData(QString path) {
	if (lastAuthService)
		lastAuthService->deleteResource(path);
}

void Profile::createWidgetAuthorization(IServices* service) {
	QFrame *frame = new QFrame(ui.frameAuthWidget);
	frame->setObjectName("frame" + service->name());
	frame->setMinimumHeight(41);
	QHBoxLayout* layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setMargin(0);
	layout->setSpacing(0);
	QPushButton* btn = new QPushButton(frame);
	btn->setObjectName("btn" + service->name());
	btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	btn->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
	connect(btn, &QPushButton::clicked, this, [this, service]() { clickedAuthorization(service); });
	btn->setText(service->widgetText());
	QFont fontBtn = btn->font();
	fontBtn.setBold(true);
	fontBtn.setPointSize(10);
	btn->setFont(fontBtn);
	QPushButton* btnIcon = new QPushButton(frame);
	btnIcon->setObjectName("btnIcon" + service->name());
	btnIcon->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
	btnIcon->setCursor(QCursor(Qt::CursorShape::PointingHandCursor));
	connect(btnIcon, &QPushButton::clicked, this, [this, service]() { clickedAuthorization(service); });
	btnIcon->setIcon(service->widgetIcon());
	layout->addWidget(btnIcon);
	layout->addWidget(btn);
	frame->setLayout(layout);
	frame->setStyleSheet("#" + frame->objectName() + "{"
		"border: none;"
		"background-color: " + service->widgetBackgroundColor().name() + ";"
		"border-radius: 5px;"
		"}");
	btn->setStyleSheet("#" + btn->objectName() + "{"
		"color:" + service->widgetTextColor().name() + ";"
		"padding-right: 30px;"
		"}");
	btnIcon->setStyleSheet("#" + btnIcon->objectName() + "{padding-left: 10px;}");

	ui.frameAuthWidget->layout()->addWidget(frame);
}

void Profile::clickedAuthorization(IServices* service) {
	INFO_MSG("Authorization to " + service->name());

	QString token = service->authorization();
	if (!token.isEmpty()) {
		INFO_MSG("Authorization to " + service->name() + " successefully");
		lastAuthService = service;
		regSettings->setValue("Services/" + service->name() + "/token", token);
		signIn();
	}
	else
		CRITICAL_MSG("Failed to get token");
}

void Profile::showProfile(Status status) {
	if (status == Status::Unknown) {
		ui.btnLogo->setIcon(QIcon(":/logo.png"));
		ui.labelName->setText("Учетная запись");
		ui.labelLogin->setText("");
		ui.frameAuthWidget->setVisible(true);
		ui.frameStorage->setVisible(false);
		ui.btnSignOut->setVisible(false);
	}
	else if (status == Status::Offline) {
		ui.frameAuthWidget->setVisible(false);
		ui.frameStorage->setVisible(true);
		ui.btnSignOut->setVisible(true);
		ui.progressBarStorageSpace->setValue(0);
		ui.labelStorageSpace->setText("Занято ? из ?");
	}
	else if (status == Status::Online) {
		ui.frameAuthWidget->setVisible(false);
		ui.frameStorage->setVisible(true);
		ui.btnSignOut->setVisible(true);
		if (lastAuthService) {
			if(!lastAuthService->avatar().isNull())
				ui.btnLogo->setIcon(lastAuthService->avatar());
			else
				ui.btnLogo->setIcon(QIcon(":/logo.png"));
			ui.labelName->setText(lastAuthService->lastName() + " " + lastAuthService->firstName());
			ui.labelLogin->setText(lastAuthService->login());
			long long totalSpace = lastAuthService->storageTotalSpace();
			long long usedSpace = lastAuthService->storageUsedSpace();
			ui.progressBarStorageSpace->setValue(usedSpace / (double)totalSpace * 100);
			QString strTotalSpace;
			QString strUsedSpace;

			if (totalSpace < 1LL * 1024 * 1024 * 1024)
				strTotalSpace = QString::number(totalSpace / (double)(1024 * 1024), 'f', 1) + " МБ";
			else if (totalSpace < 1LL * 1024 * 1024 * 1024 * 1024)
				strTotalSpace = QString::number(totalSpace / (double)(1LL * 1024 * 1024 * 1024), 'f', 1) + " ГБ";
			else
				strTotalSpace = QString::number(totalSpace / (double)(1LL * 1024 * 1024 * 1024 * 1024), 'f', 1) + " ТБ";

			if (usedSpace < 1LL * 1024 * 1024 * 1024)
				strUsedSpace = QString::number(usedSpace / (double)(1024 * 1024), 'f', 1) + " МБ";
			else if (usedSpace < 1LL * 1024 * 1024 * 1024 * 1024)
				strUsedSpace = QString::number(usedSpace / (double)(1LL * 1024 * 1024 * 1024), 'f', 1) + " ГБ";
			else
				strUsedSpace = QString::number(usedSpace / (double)(1LL * 1024 * 1024 * 1024 * 1024), 'f', 1) + " ТБ";

			ui.labelStorageSpace->setText("Занято " + strUsedSpace + " из " + strTotalSpace);
		}
	}
}
