/// \file
/// \author
///
/// \brief Implementation of text snippet fragment class.
///
/// Copyright (c). All rights reserved.
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.


#include "stdafx.h"
#include "TextSnippetFragment.h"
#include "BeeftextUtils.h"
#include <XMiLib/Exception.h>


namespace {


#ifdef Q_OS_WINDOWS


QList<WORD> const kModifierKeys = {
    VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU,
    VK_LSHIFT, VK_RSHIFT, VK_LWIN, VK_RWIN
};


bool isExtendedKey(WORD virtualKey) {
    return (virtualKey == VK_RCONTROL) || (virtualKey == VK_RMENU)
           || (virtualKey == VK_LWIN) || (virtualKey == VK_RWIN);
}


void appendVirtualKeyEvent(std::vector<INPUT> &events, WORD virtualKey, bool pressed) {
    INPUT event = {};
    event.type = INPUT_KEYBOARD;
    event.ki.wVk = virtualKey;
    event.ki.wScan = static_cast<WORD>(MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC));
    event.ki.dwFlags = pressed ? 0 : KEYEVENTF_KEYUP;
    if (isExtendedKey(virtualKey))
        event.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    events.push_back(event);
}


void appendUnicodeCharacter(std::vector<INPUT> &events, QChar character) {
    INPUT keyDown = {};
    keyDown.type = INPUT_KEYBOARD;
    keyDown.ki.wScan = character.unicode();
    keyDown.ki.dwFlags = KEYEVENTF_UNICODE;

    INPUT keyUp = keyDown;
    keyUp.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

    events.push_back(keyDown);
    events.push_back(keyUp);
}


void typeRestrictedText(QString const &text) {
    QList<WORD> pressedModifiers;
    for (WORD const modifier: kModifierKeys) {
        if (GetKeyState(modifier) < 0)
            pressedModifiers.push_back(modifier);
    }

    std::vector<INPUT> events;
    events.reserve(static_cast<size_t>((pressedModifiers.size() * 2) + (text.size() * 2)));

    for (WORD const modifier: pressedModifiers)
        appendVirtualKeyEvent(events, modifier, false);

    for (qsizetype index = 0; index < text.size(); ++index) {
        QChar const character = text[index];
        if ((character == QChar::CarriageReturn) && ((index + 1) < text.size())
            && (text[index + 1] == QChar::LineFeed))
            continue;

        if ((character == QChar::LineFeed) || (character == QChar::CarriageReturn)) {
            appendVirtualKeyEvent(events, VK_RETURN, true);
            appendVirtualKeyEvent(events, VK_RETURN, false);
        } else {
            appendUnicodeCharacter(events, character);
        }
    }

    for (WORD const modifier: pressedModifiers)
        appendVirtualKeyEvent(events, modifier, true);

    if (events.empty())
        return;

    UINT const eventCount = static_cast<UINT>(events.size());
    UINT const sent = SendInput(eventCount, events.data(), sizeof(INPUT));
    if (sent == eventCount)
        return;

    DWORD const error = GetLastError();
    std::vector<INPUT> restoreEvents;
    restoreEvents.reserve(static_cast<size_t>(pressedModifiers.size()));
    for (WORD const modifier: pressedModifiers)
        appendVirtualKeyEvent(restoreEvents, modifier, true);
    if (!restoreEvents.empty())
        SendInput(static_cast<UINT>(restoreEvents.size()), restoreEvents.data(), sizeof(INPUT));

    throw xmilib::Exception(QString("Could not type restricted snippet: sent %1 of %2 events (Windows error %3).")
                                .arg(sent).arg(eventCount).arg(error));
}


#endif // Q_OS_WINDOWS


} // anonymous namespace


//****************************************************************************************************************************************************
/// \param[in] text The text.
//****************************************************************************************************************************************************
TextSnippetFragment::TextSnippetFragment(QString const &text)
    : SnippetFragment()
    , text_(text) {
}


//****************************************************************************************************************************************************
/// \return the type of the snippet fragment.
//****************************************************************************************************************************************************
SnippetFragment::EType TextSnippetFragment::type() const {
    return EType::Text;
}


//****************************************************************************************************************************************************
/// \return a string describing the snippet fragment.
//****************************************************************************************************************************************************
QString TextSnippetFragment::toString() const {
    return "Text fragment: " + text_;
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void TextSnippetFragment::render() const {
    if constexpr (constants::kRestrictedBuild) {
#ifdef Q_OS_WINDOWS
        typeRestrictedText(text_);
#else
        insertText(text_);
#endif
        return;
    }

    insertText(text_);
}
