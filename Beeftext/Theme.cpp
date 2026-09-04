/// \file
/// \author 
///
/// \brief Implementation of theme related functions.
///  
/// Copyright (c) . All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information. 


#include "stdafx.h"
#include "Theme.h"
#include "BeeftextGlobals.h"
#include <XMiLib/Exception.h>


QString loadStylesheetFile(QString const &path); ///< Load a stylesheet from file. The function throws an exception if an error occur.


namespace {


QPalette const &nativeApplicationPalette() {
    static QPalette const palette = qApp->palette();
    return palette;
}


void setPaletteColor(QPalette &palette, QPalette::ColorRole role, QColor const &color) {
    palette.setColor(role, color);
}


QPalette lightPalette() {
    QPalette palette = nativeApplicationPalette();
    setPaletteColor(palette, QPalette::Window, QColor("#f3f3f3"));
    setPaletteColor(palette, QPalette::WindowText, QColor("#1f1f1f"));
    setPaletteColor(palette, QPalette::Base, QColor("#ffffff"));
    setPaletteColor(palette, QPalette::AlternateBase, QColor("#f7f7f7"));
    setPaletteColor(palette, QPalette::Text, QColor("#1f1f1f"));
    setPaletteColor(palette, QPalette::Button, QColor("#fbfbfb"));
    setPaletteColor(palette, QPalette::ButtonText, QColor("#1f1f1f"));
    setPaletteColor(palette, QPalette::BrightText, QColor("#ffffff"));
    setPaletteColor(palette, QPalette::Light, QColor("#ffffff"));
    setPaletteColor(palette, QPalette::Midlight, QColor("#e8e8e8"));
    setPaletteColor(palette, QPalette::Mid, QColor("#9a9a9a"));
    setPaletteColor(palette, QPalette::Dark, QColor("#6f6f6f"));
    setPaletteColor(palette, QPalette::Shadow, QColor("#3f3f3f"));
    setPaletteColor(palette, QPalette::Highlight, QColor("#0067c0"));
    setPaletteColor(palette, QPalette::HighlightedText, QColor("#ffffff"));
    setPaletteColor(palette, QPalette::Link, QColor("#0067c0"));
    setPaletteColor(palette, QPalette::LinkVisited, QColor("#744da9"));
    setPaletteColor(palette, QPalette::ToolTipBase, QColor("#ffffff"));
    setPaletteColor(palette, QPalette::ToolTipText, QColor("#1f1f1f"));
    setPaletteColor(palette, QPalette::PlaceholderText, QColor("#6b6b6b"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    setPaletteColor(palette, QPalette::Accent, QColor("#0067c0"));
#endif

    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#6b6b6b"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#6b6b6b"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#6b6b6b"));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor("#e8e8e8"));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor("#e5e5e5"));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor("#b8b8b8"));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor("#6b6b6b"));
    palette.setColor(QPalette::Disabled, QPalette::Link, QColor("#6b6b6b"));
    palette.setColor(QPalette::Disabled, QPalette::LinkVisited, QColor("#6b6b6b"));
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, QColor("#858585"));
    return palette;
}


QPalette darkPalette() {
    QPalette palette = nativeApplicationPalette();
    setPaletteColor(palette, QPalette::Window, QColor("#202020"));
    setPaletteColor(palette, QPalette::WindowText, QColor("#f2f2f2"));
    setPaletteColor(palette, QPalette::Base, QColor("#2b2b2b"));
    setPaletteColor(palette, QPalette::AlternateBase, QColor("#323232"));
    setPaletteColor(palette, QPalette::Text, QColor("#f2f2f2"));
    setPaletteColor(palette, QPalette::Button, QColor("#333333"));
    setPaletteColor(palette, QPalette::ButtonText, QColor("#f2f2f2"));
    setPaletteColor(palette, QPalette::BrightText, QColor("#ffffff"));
    setPaletteColor(palette, QPalette::Light, QColor("#626262"));
    setPaletteColor(palette, QPalette::Midlight, QColor("#4a4a4a"));
    setPaletteColor(palette, QPalette::Mid, QColor("#3d3d3d"));
    setPaletteColor(palette, QPalette::Dark, QColor("#151515"));
    setPaletteColor(palette, QPalette::Shadow, QColor("#080808"));
    setPaletteColor(palette, QPalette::Highlight, QColor("#60cdff"));
    setPaletteColor(palette, QPalette::HighlightedText, QColor("#111111"));
    setPaletteColor(palette, QPalette::Link, QColor("#75b6e7"));
    setPaletteColor(palette, QPalette::LinkVisited, QColor("#c8a7e8"));
    setPaletteColor(palette, QPalette::ToolTipBase, QColor("#333333"));
    setPaletteColor(palette, QPalette::ToolTipText, QColor("#f2f2f2"));
    setPaletteColor(palette, QPalette::PlaceholderText, QColor("#a0a0a0"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    setPaletteColor(palette, QPalette::Accent, QColor("#60cdff"));
#endif

    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#858585"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#858585"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#858585"));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor("#262626"));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor("#292929"));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor("#454545"));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor("#858585"));
    palette.setColor(QPalette::Disabled, QPalette::Link, QColor("#858585"));
    palette.setColor(QPalette::Disabled, QPalette::LinkVisited, QColor("#858585"));
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, QColor("#707070"));
    return palette;
}


} // Anonymous namespace


//****************************************************************************************************************************************************
/// \param[in] theme The theme.
/// \return The display name of the theme.
//****************************************************************************************************************************************************
QString themeName(ETheme theme) {
    switch (theme) {
    case ETheme::Light:
        return QObject::tr("light");
    case ETheme::Dark:
        return QObject::tr("dark");
    case ETheme::Count:
    default:
        return QObject::tr("unknown");
    }
}


//****************************************************************************************************************************************************
/// \param[in] combo The combo box.
//****************************************************************************************************************************************************
void fillThemeComboBox(QComboBox &combo) {
    QSignalBlocker blocker(&combo);
    combo.clear();
    for (qint32 i = 0; i < static_cast<qint32>(ETheme::Count); ++i) {
        QString name = themeName(static_cast<ETheme>(i));
        if (name.length() > 0)
            name[0] = name[0].toUpper();
        combo.addItem(name, i);
    }
}


//****************************************************************************************************************************************************
/// \param[in] theme The theme.
/// \param[in] combo The combo box.
//****************************************************************************************************************************************************
void selectThemeInCombo(ETheme theme, QComboBox &combo) {
    for (qint32 i = 0; i < combo.count(); ++i) {
        bool ok = false;
        ETheme const itemTheme = static_cast<ETheme>(combo.itemData(i).toInt(&ok));
        if (!ok)
            continue;
        if (itemTheme == theme)
            combo.setCurrentIndex(i);
    }
}


//****************************************************************************************************************************************************
/// \param[in] combo The combo box.
/// \return The currently selected item in a combo box.
//****************************************************************************************************************************************************
ETheme selectedThemeInCombo(QComboBox const &combo) {
    qint32 index = qMax<qint32>(0, combo.currentIndex());
    if (index >= static_cast<qint32>(ETheme::Count))
        index = 0;
    return static_cast<ETheme>(index);
}


//****************************************************************************************************************************************************
/// \param[in] useCustomTheme Does the application use a custom theme
/// \param[in] theme The theme.
//****************************************************************************************************************************************************
void applyThemePreferences(bool useCustomTheme, ETheme theme) {
    try {
        if (!useCustomTheme) {
            qApp->setPalette(nativeApplicationPalette());
            qApp->setStyleSheet(loadStylesheetFile(":/MainWindow/Resources/StyleNoCustom.qss"));
            return;
        }

        qApp->setPalette((ETheme::Dark == theme) ? darkPalette() : lightPalette());
        qApp->setStyleSheet(loadStylesheetFile(":/MainWindow/Resources/StyleCommon.qss") + "\n" +
                            loadStylesheetFile(QString(":/MainWindow/Resources/Style%1.qss").arg((ETheme::Dark == theme)
                                                                                                 ? "Dark" : "Light")));
    }
    catch (xmilib::Exception const &e) {
        QString const &msg = e.qwhat();
        if (!msg.isEmpty())
            globals::debugLog().addWarning(msg);
        qApp->setPalette(nativeApplicationPalette());
        qApp->setStyleSheet(QString());
    }

}


//****************************************************************************************************************************************************
/// \param[in] path The path of the style sheet to load.
/// \return The loaded style sheet.
//****************************************************************************************************************************************************
QString loadStylesheetFile(QString const &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        throw xmilib::Exception(QString("Could not load stylesheet %1").arg(path));
    return QString::fromUtf8(file.readAll());
}
