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
