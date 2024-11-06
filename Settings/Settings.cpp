#include "Settings.h"

Settings::Settings(QSettings* regset, QWidget *parent)
	: QWidget(parent)
{
	ui.setupUi(this);
	regSettings = regset;

    ui.stackedWidgetSettings->setCurrentIndex(0);

    ui.lineEditMaxSyncSize->setValidator(new QIntValidator(1, 9999));
    ui.checkBoxStartUpMinimized->setDisabled(false);
    ui.checkBoxBackgroundWork->setDisabled(false);
    connect(ui.checkBoxStartUp, &QCheckBox::stateChanged, this, [this](int state) {
        if (state == 0) {
            ui.checkBoxStartUpMinimized->setDisabled(true);
            ui.checkBoxStartUpMinimized->setChecked(false);
        }
        else {
            ui.checkBoxStartUpMinimized->setDisabled(false);
        }
        });
    connect(ui.checkBoxStartUpMinimized, &QCheckBox::stateChanged, this, [this](int state) {
        if (state == 0) {
            ui.checkBoxBackgroundWork->setDisabled(false);
        }
        else {
            ui.checkBoxBackgroundWork->setDisabled(true);
            ui.checkBoxBackgroundWork->setChecked(true);
        }
        });

    // registry
    QString pathApp = QCoreApplication::applicationFilePath();
    if (!regSettings->value("Settings/StartUp").isValid() || pathApp != regSettings->value("Settings/StartUpPath").toString()) {
        setStartUp(true, true);

        regSettings->setValue("Settings/StartUp", true);
        regSettings->setValue("Settings/StartUpPath", pathApp);
        regSettings->setValue("Settings/StartUpMinimized", true);
    }

    startup = regSettings->value("Settings/StartUp").toBool();
    startupMinimized = regSettings->value("Settings/StartUpMinimized").toBool();

    QVariant variantBackgroundWork = regSettings->value("Settings/BackgroundWork");
    if(variantBackgroundWork.isValid())
        backgroundWork = variantBackgroundWork.toBool();
    else {
        backgroundWork = defaultBackgroundWork;
        regSettings->setValue("Settings/BackgroundWork", defaultBackgroundWork);
    }

    QVariant variantMaxSyncSize = regSettings->value("Settings/MaxSyncSize");
    if (variantMaxSyncSize.isValid())
        maxSyncSize = variantMaxSyncSize.toInt();
    else {
        maxSyncSize = defaultMaxSyncSize;
        regSettings->setValue("Settings/MaxSyncSize", defaultMaxSyncSize);
    }

    QVariant variantAutoSync = regSettings->value("Settings/AutoSync");
    if (variantAutoSync.isValid())
        autoSync = variantAutoSync.toBool();
    else {
        autoSync = defaultAutoSync;
        regSettings->setValue("Settings/AutoSync", defaultAutoSync);
    }

    QVariant variantAutoSignIn = regSettings->value("Settings/AutoSignIn");
    if (variantAutoSignIn.isValid())
        autoSignIn = variantAutoSignIn.toBool();
    else {
        autoSignIn = defaultAutoSignIn;
        regSettings->setValue("Settings/AutoSignIn", defaultAutoSignIn);
    }

    // ui
    ui.checkBoxStartUp->setChecked(startup);
    ui.checkBoxStartUpMinimized->setChecked(startupMinimized);
    if (startupMinimized) {
        ui.checkBoxBackgroundWork->setDisabled(true);
        ui.checkBoxBackgroundWork->setChecked(true);
    }
    else
        ui.checkBoxBackgroundWork->setChecked(backgroundWork);

    ui.lineEditMaxSyncSize->setText(QString::number(maxSyncSize));
    ui.checkBoxAutoSync->setChecked(autoSync);

    ui.checkBoxAutoSignIn->setChecked(autoSignIn);
}

Settings::~Settings()
{
}

bool Settings::getStartUp() {
    return startup;
}
bool Settings::getStartUpMinimized() {
    return startupMinimized;
}
bool Settings::getBackgroundWork() {
    return backgroundWork;
}
int Settings::getMaxSyncSize() {
    return maxSyncSize;
}
bool Settings::getAutoSync() {
    return autoSync;
}
bool Settings::getAutoSignIn() {
    return autoSignIn;
}

void Settings::on_btnOk_clicked() {
    on_btnApply_clicked();
    hide();
}

void Settings::on_btnApply_clicked() {
    if (startup != ui.checkBoxStartUp->isChecked() || startupMinimized != ui.checkBoxStartUpMinimized->isChecked()) {
        setStartUp(ui.checkBoxStartUp->isChecked(), ui.checkBoxStartUpMinimized->isChecked());

        regSettings->setValue("Settings/StartUp", ui.checkBoxStartUp->isChecked());
        regSettings->setValue("Settings/StartUpPath", QCoreApplication::applicationFilePath());
        regSettings->setValue("Settings/StartUpMinimized", ui.checkBoxStartUpMinimized->isChecked());
    }
    startup = ui.checkBoxStartUp->isChecked();
    startupMinimized = ui.checkBoxStartUpMinimized->isChecked();
    backgroundWork = ui.checkBoxBackgroundWork->isChecked();

    maxSyncSize = ui.lineEditMaxSyncSize->text().toInt();
    autoSync = ui.checkBoxAutoSync->isChecked();

    autoSignIn = ui.checkBoxAutoSignIn->isChecked();

    regSettings->setValue("Settings/BackgroundWork", backgroundWork);
    regSettings->setValue("Settings/MaxSyncSize", maxSyncSize);
    regSettings->setValue("Settings/AutoSync", autoSync);
    regSettings->setValue("Settings/AutoSignIn", autoSignIn);
}

void Settings::on_btnCancel_clicked() {
    hide();
}

void Settings::on_btnReset_clicked() {
    startup = defaultStartup;
    startupMinimized = defaultStartupMinimized;
    backgroundWork = defaultBackgroundWork;

    maxSyncSize = defaultMaxSyncSize;
    autoSync = defaultAutoSync;

    autoSignIn = defaultAutoSignIn;

    on_btnApply_clicked();
}

void Settings::on_treeWidgetSettings_itemClicked(QTreeWidgetItem* item, int column) {
    if (item->text(column) == "Основные" || item->text(column) == "Внешний вид") {
        ui.stackedWidgetSettings->setCurrentWidget(ui.pageAppearance);
    }
    else if (item->text(column) == "Клавиатура") {
        ui.stackedWidgetSettings->setCurrentWidget(ui.pageKeyboard);
    }
    else if (item->text(column) == "Синхронизация") {
        ui.stackedWidgetSettings->setCurrentWidget(ui.pageSync);
    }
    else if (item->text(column) == "Сервисы") {
        ui.stackedWidgetSettings->setCurrentWidget(ui.pageServices);
    }
    else if (item->text(column) == "Системные") {
        ui.stackedWidgetSettings->setCurrentWidget(ui.pageSystem);
    }
}

void Settings::on_lineEditFindSettings_textChanged(const QString& text) {

    // алгоритм поиска настроек
    for (int i = 0; i < ui.treeWidgetSettings->topLevelItemCount(); ++i) {
        QTreeWidgetItem* itemTree = ui.treeWidgetSettings->topLevelItem(i);
        bool findedChild = false;
        for (int j = 0; j < itemTree->childCount(); ++j) {
            if (itemTree->child(j)->text(0).contains(text, Qt::CaseInsensitive)) {
                itemTree->child(j)->setHidden(false);
                ui.treeWidgetSettings->expandItem(itemTree);
                findedChild = true;
            }
            else {
                if (!findedChild)
                    ui.treeWidgetSettings->collapseItem(itemTree);

                if (!itemTree->text(0).contains(text, Qt::CaseInsensitive))
                    itemTree->child(j)->setHidden(true);
            }
        }

        if (itemTree->text(0).contains(text, Qt::CaseInsensitive) || findedChild)
            itemTree->setHidden(false);
        else
            itemTree->setHidden(true);
    }

    if (text.isEmpty())
        ui.treeWidgetSettings->collapseAll();
}

void Settings::setStartUp(bool value, bool minimized) {
    if (value) {
        QString appPath = QCoreApplication::applicationFilePath().replace('/', '\\');
        
        if (minimized) {
            QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
            settings.setValue(appName, "\"" + appPath + "\" --hide");
        }
        else {
            QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
            settings.setValue(appName, appPath);
        }
    }
    else {
        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        settings.remove(appName);
    }
}

void Settings::showEvent(QShowEvent* event) {
    ui.checkBoxStartUp->setChecked(startup);
    ui.checkBoxStartUpMinimized->setChecked(startupMinimized);
    if (startupMinimized) {
        ui.checkBoxBackgroundWork->setDisabled(true);
        ui.checkBoxBackgroundWork->setChecked(true);
    }
    else
        ui.checkBoxBackgroundWork->setChecked(backgroundWork);

    ui.lineEditMaxSyncSize->setText(QString::number(maxSyncSize));
    ui.checkBoxAutoSync->setChecked(autoSync);

    ui.checkBoxAutoSignIn->setChecked(autoSignIn);

    QWidget::showEvent(event);
}
