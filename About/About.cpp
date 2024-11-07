#include "About.h"

About::About(QSettings* regset, QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	regSettings = regset;
}

About::~About()
{
}

void About::setVersion(QString version) {
	ui.labelVersion->setText(version);
}
void About::setBuildDate(QString buildDate) {
	ui.labelBuildDate->setText(buildDate);
}
void About::setVersionAPI(QString versionAPI) {
	ui.labelVersionAPI->setText(versionAPI);
}
void About::setContact(QString contact) {
	ui.labelContact->setText(contact);
}

QString About::version() {
	return ui.labelVersion->text();
}
QString About::buildDate() {
	return ui.labelBuildDate->text();
}
QString About::versionAPI() {
	return ui.labelVersionAPI->text();
}
QString About::contact() {
	return ui.labelContact->text();
}