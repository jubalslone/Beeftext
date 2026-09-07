/// \file
/// \author Xavier Michelon
///
/// \brief Implementation of system-appearance support.
///
/// Copyright (c) Xavier Michelon. All rights reserved.
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.


#include "stdafx.h"
#include "Theme.h"
#include "BeeftextGlobals.h"
#include <XMiLib/Exception.h>


namespace {


QString loadStylesheetFile(QString const &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        throw xmilib::Exception(QString("Could not load stylesheet %1").arg(path));
    return QString::fromUtf8(file.readAll());
}


} // Anonymous namespace


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void applySystemTheme() {
    try {
        // Colors and native control rendering come exclusively from the Windows/Qt
        // system palette. This stylesheet only frames the frameless picker window.
        qApp->setStyleSheet(loadStylesheetFile(":/MainWindow/Resources/StyleNoCustom.qss"));
    }
    catch (xmilib::Exception const &e) {
        QString const &msg = e.qwhat();
        if (!msg.isEmpty())
            globals::debugLog().addWarning(msg);
        qApp->setStyleSheet(QString());
    }
}
