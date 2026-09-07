/// \file
/// \author 
///
/// \brief Implementation of the language preference pane
///  
/// Copyright (c) . All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information. 


#include "stdafx.h"
#include "PrefPaneAppearance.h"
#include "I18nManager.h"
#include "BeeftextGlobals.h"


//****************************************************************************************************************************************************
/// \param[in] parent The parent widget of the frame.
//****************************************************************************************************************************************************
PrefPaneAppearance::PrefPaneAppearance(QWidget *parent)
    : PrefPane(parent)
    , prefs_(PreferencesManager::instance()) {
    ui_.setupUi(this);

    connect(ui_.buttonRefresh, &QPushButton::clicked, this, &PrefPaneAppearance::onRefreshLanguageList);
    connect(ui_.buttonTranslationFolder, &QPushButton::clicked, this, &PrefPaneAppearance::onOpenTranslationFolder);
    connect(ui_.comboLocale, &QComboBox::currentIndexChanged, this, &PrefPaneAppearance::onComboLanguageValueChanged);

    I18nManager::instance().fillLocaleCombo(*ui_.comboLocale);
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void PrefPaneAppearance::load() const {
    QSignalBlocker blocker = QSignalBlocker(ui_.comboLocale);
    this->onRefreshLanguageList();
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void PrefPaneAppearance::onRefreshLanguageList() const {
    I18nManager &i18NManager = I18nManager::instance();
    QLocale const currentLocale = I18nManager::locale();
    i18NManager.refreshSupportedLocalesList();
    i18NManager.fillLocaleCombo(*ui_.comboLocale);
    QLocale const validatedLocale = i18NManager.validateLocale(currentLocale);
    I18nManager::selectLocaleInCombo(validatedLocale, *ui_.comboLocale);
    i18NManager.setLocale(validatedLocale);
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void PrefPaneAppearance::onComboLanguageValueChanged(int) const {
    prefs_.setLocale(I18nManager::instance().getSelectedLocaleInCombo(*ui_.comboLocale));
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void PrefPaneAppearance::onOpenTranslationFolder() {
    QString const path = globals::userTranslationRootFolderPath();
    if (QDir(path).exists())
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}


//****************************************************************************************************************************************************
/// \param[in] event The event.
//****************************************************************************************************************************************************
void PrefPaneAppearance::changeEvent(QEvent *event) {
    if (QEvent::LanguageChange == event->type()) {
        ui_.retranslateUi(this);
    }
    PrefPane::changeEvent(event);
}
