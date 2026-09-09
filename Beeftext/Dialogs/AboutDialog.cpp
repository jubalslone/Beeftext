/// \file
/// \author Xavier Michelon
///
/// \brief Implementation of the 'About' dialog class
///  
/// Copyright (c) Xavier Michelon. All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.  


#include "stdafx.h"
#include "AboutDialog.h"
#include "BeeftextConstants.h"
#include "BeeftextGlobals.h"
#include "BeeftextUtils.h"
#include <XMiLib/XMiLibConstants.h>


//****************************************************************************************************************************************************
/// \param[in] parent The parent widget of the dialog
//****************************************************************************************************************************************************
AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent, xmilib::constants::kDefaultDialogFlags)
    , ui_() {
    ui_.setupUi(this);
    connect(ui_.buttonClose, &QPushButton::clicked, this, &AboutDialog::accept);
    this->completeText();
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void AboutDialog::completeText() const {
    ui_.labelProductName->setText(tr("%1 %2").arg(constants::kApplicationName, constants::kProductVersion));
    ui_.labelPortableEdition->setVisible(isInPortableMode());

    QString const applicationDir = QCoreApplication::applicationDirPath();
    QString const licenseUrl = QUrl::fromLocalFile(QDir(applicationDir).absoluteFilePath("LICENSE")).toString();
    QString const noticesUrl = QUrl::fromLocalFile(QDir(applicationDir).absoluteFilePath("THIRD_PARTY_NOTICES.md")).toString();
    ui_.labelLinks->setText(tr(
        R"(<a href="https://github.com/jubalslone/lean-beeftext">Project Repository</a> &nbsp;·&nbsp; )"
        R"(<a href="https://github.com/xmichelo/Beeftext">Upstream Beeftext</a> &nbsp;·&nbsp; )"
        R"(<a href="%1">MIT License</a> &nbsp;·&nbsp; <a href="%2">Third-Party Notices</a>)")
        .arg(licenseUrl.toHtmlEscaped(), noticesUrl.toHtmlEscaped()));
    ui_.labelBuildInfo->setText(tr("Build: %1").arg(globals::getBuildInfo()));
}


//****************************************************************************************************************************************************
/// \param[in] event The event
//****************************************************************************************************************************************************
void AboutDialog::changeEvent(QEvent *event) {
    if (QEvent::LanguageChange == event->type()) {
        ui_.retranslateUi(this);
        this->completeText();
    }
    QDialog::changeEvent(event);
}
