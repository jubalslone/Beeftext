/// \file
/// \author 
///
/// \brief Implementation of auto start related function.
///  
/// Copyright (c) . All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information. 


#include "stdafx.h"
#include "AutoStart.h"
#include "BeeftextUtils.h"
#include "BeeftextGlobals.h"
#include "BeeftextConstants.h"
#include "Preferences/PreferencesManager.h"


namespace {


QString const kKeyAutoStart = R"(HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run)"; ///< The registry key for autostart


}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void applyAutostartParameters() {
    if (isInPortableMode())
        return;
    bool const autoStart = PreferencesManager::instance().autoStartAtLogin();
    QString registeredAppPath;
    bool const hasRegisteredApp = registeredApplicationForAutostart(registeredAppPath);
    registeredAppPath = QFileInfo(registeredAppPath).absoluteFilePath();
    xmilib::DebugLog &log = globals::debugLog();
    if (autoStart) {
        log.addInfo("Auto-start is enabled.");
        QString const &installedAppPath = QFileInfo(installedApplicationPath()).absoluteFilePath();
        if (installedAppPath.isEmpty()) {
            log.addError("Lean Beeftext is not properly installed (installedAppPath is empty). The application cannot be "
                         "registered for auto-start.");
            return;
        }
        if (!QFileInfo(installedAppPath).exists()) {
            log.addError(QString("Lean Beeftext is not properly installed (installedAppPath points to a non-existing "
                                 "file '%1'). The application cannot be registered for auto-start.").arg(installedAppPath));
            return;
        }

        if (hasRegisteredApp) {
            if (installedAppPath == registeredAppPath) {
                log.addInfo(QString("Lean Beeftext is already properly registered in system for auto-start. Path is '%1'.")
                    .arg(QDir::toNativeSeparators(registeredAppPath)));
                return;
            }
            registerApplicationForAutoStart(installedAppPath);
            log.addWarning(QString("The path of the registered instance of Lean Beeftext '%1' is invalid and has "
                                   "been modified to '%2'.").arg(registeredAppPath).arg(installedAppPath));
            return;
        }
        registerApplicationForAutoStart(installedAppPath);
        log.addInfo(QString("Lean Beeftext has been registered for auto-start. Application path is '%1'.")
            .arg(QDir::toNativeSeparators(installedAppPath)));
        return;
    }

    log.addInfo("Auto-start is disabled.");
    if (hasRegisteredApp) {
        log.addInfo(QString("Removing registered app from Windows auto-start '%1'")
            .arg(QDir::toNativeSeparators(registeredAppPath)));
        unregisterApplicationFromAutoStart();
    }
}


//****************************************************************************************************************************************************
/// \return The canonical path of the currently running executable.
//****************************************************************************************************************************************************
QString installedApplicationPath() {
    QFileInfo const exeInfo(QCoreApplication::applicationFilePath());
    QString const canonicalPath = exeInfo.canonicalFilePath();
    return canonicalPath.isEmpty() ? exeInfo.absoluteFilePath() : canonicalPath;
}


//****************************************************************************************************************************************************
/// \param[out] outPath The path of the application registered for autostart.
//****************************************************************************************************************************************************
bool registeredApplicationForAutostart(QString &outPath) {
    QSettings const settings(kKeyAutoStart, QSettings::NativeFormat);
    QString const key = settings.contains(constants::kApplicationName) ? constants::kApplicationName
        : (settings.contains(constants::kSettingsApplicationName) ? constants::kSettingsApplicationName : QString());
    if (key.isEmpty())
        return false;
    QVariant const v = settings.value(key);
    if (!v.canConvert<QString>())
        return false;
    QString value = v.toString().trimmed();
    if (value.startsWith('"') && value.endsWith('"'))
        value = value.mid(1, value.size() - 2);
    outPath = QDir::fromNativeSeparators(value);
    return true;
}


//****************************************************************************************************************************************************
/// \param[in] appPath The path of the application.
//****************************************************************************************************************************************************
void registerApplicationForAutoStart(QString const &appPath) {
    QSettings settings(kKeyAutoStart, QSettings::NativeFormat);
    settings.setValue(constants::kApplicationName, QString("\"%1\"").arg(QDir::toNativeSeparators(appPath)));
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void unregisterApplicationFromAutoStart() {
    QSettings settings(kKeyAutoStart, QSettings::NativeFormat);
    settings.remove(constants::kApplicationName);
}
