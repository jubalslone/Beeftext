/// \file
///
/// \brief Fail-closed parsing helpers for the TLF restricted build.
///
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.


#include "TLFSafeBuild.h"

#include <QChar>
#include <QRegularExpression>
#include <QSettings>
#include <limits>


namespace {


QString const kCursorVariable = "#{cursor}";


QString visibleCodeUnit(quint16 value) {
    return QStringLiteral("\\u") + QStringLiteral("%1").arg(value, 4, 16, QChar('0')).toUpper();
}


bool isHighSurrogate(quint16 value) {
    return (value >= 0xd800) && (value <= 0xdbff);
}


bool isLowSurrogate(quint16 value) {
    return (value >= 0xdc00) && (value <= 0xdfff);
}


bool isSafeCursorSuffix(QString const &suffix, bool allowRealLineBreaks) {
    if (suffix.size() > std::numeric_limits<qint32>::max())
        return false;

    // Cursor movement is implemented with Left-arrow events. Limiting the suffix to
    // printable ASCII makes the one-event-per-character bound predictable across the
    // Windows controls used for QA. Unicode grapheme movement is application-specific.
    for (QChar const character: suffix) {
        quint16 const value = character.unicode();
        if (allowRealLineBreaks && (value == '\n'))
            continue;
        if ((value < 0x20) || (value > 0x7e))
            return false;
    }
    return true;
}


} // anonymous namespace


namespace tlf {


ERestrictedVariable classifyVariable(QString const &variable) {
    if (variable == "cursor")
        return ERestrictedVariable::Cursor;
    if (variable == "date")
        return ERestrictedVariable::Date;
    if (variable == "time")
        return ERestrictedVariable::Time;
    if (variable == "dateTime")
        return ERestrictedVariable::DateTime;

    if (variable.startsWith("dateTime:")) {
        QString const payload = variable.mid(QStringLiteral("dateTime:").size());
        if (!payload.startsWith('+') && !payload.startsWith('-'))
            return ERestrictedVariable::CustomDateTime;

        static QRegularExpression const shiftedDateTime(R"(^([+-]\d+[yMwdhmsz])+:(.*)$)");
        return shiftedDateTime.match(payload).hasMatch() ? ERestrictedVariable::CustomDateTime
                                                          : ERestrictedVariable::Blocked;
    }
    if (variable.startsWith("combo:"))
        return ERestrictedVariable::Combo;
    if (variable.startsWith("upper:"))
        return ERestrictedVariable::Upper;
    if (variable.startsWith("lower:"))
        return ERestrictedVariable::Lower;
    if (variable.startsWith("trim:"))
        return ERestrictedVariable::Trim;
    if (variable.startsWith("input:"))
        return ERestrictedVariable::Input;
    return ERestrictedVariable::Blocked;
}


QString sanitizeText(QString const &text, bool allowRealLineBreaks) {
    QString result;
    result.reserve(text.size());

    for (qsizetype index = 0; index < text.size(); ++index) {
        QChar const character = text[index];
        quint16 const value = character.unicode();

        if (value == '\r') {
            if (((index + 1) < text.size()) && (text[index + 1] == QChar::LineFeed))
                ++index;
            result += allowRealLineBreaks ? QStringLiteral("\n") : QStringLiteral("\\n");
            continue;
        }
        if (value == '\n') {
            result += allowRealLineBreaks ? QStringLiteral("\n") : QStringLiteral("\\n");
            continue;
        }
        if (value == '\t') {
            result += QStringLiteral("\\t");
            continue;
        }

        // Unicode line and paragraph separators can also be interpreted as submission
        // controls, depending on the target application.
        if ((value == 0x2028) || (value == 0x2029)) {
            result += visibleCodeUnit(value);
            continue;
        }

        if (isHighSurrogate(value)) {
            if (((index + 1) < text.size()) && isLowSurrogate(text[index + 1].unicode())) {
                result += character;
                result += text[++index];
            } else {
                result += visibleCodeUnit(value);
            }
            continue;
        }
        if (isLowSurrogate(value)) {
            result += visibleCodeUnit(value);
            continue;
        }

        if (character.category() == QChar::Other_Control) {
            result += visibleCodeUnit(value);
            continue;
        }
        result += character;
    }
    return result;
}


RestrictedSnippet prepareSnippet(QString const &text, bool allowRealLineBreaks) {
    RestrictedSnippet result;
    result.text = sanitizeText(text, allowRealLineBreaks);

    // Upstream v16 evaluates nested combos before cursor handling and uses the
    // last cursor marker as the final caret position. Matching that rule makes
    // repeated child-combo expansion useful while keeping the Left-event count
    // bounded by the suffix that was actually inserted.
    qsizetype const markerIndex = result.text.lastIndexOf(kCursorVariable);
    if (markerIndex < 0)
        return result;

    QString const suffix = result.text.mid(markerIndex + kCursorVariable.size());
    if (!isSafeCursorSuffix(suffix, allowRealLineBreaks)) {
        result.cursorSyntaxRejected = true;
        return result;
    }

    result.text.remove(kCursorVariable, Qt::CaseSensitive);
    result.cursorLeftCount = static_cast<qint32>(suffix.size());
    return result;
}


bool readAllowRealLineBreaksInSnippets(QSettings const &settings) {
    QVariant const value = settings.value(QString::fromLatin1(kAllowRealLineBreaksInSnippetsSettingKey));
    if (!value.isValid())
        return kDefaultAllowRealLineBreaksInSnippets;

    QString const normalized = value.toString().trimmed().toLower();
    if ((normalized == QStringLiteral("true")) || (normalized == QStringLiteral("1")))
        return true;
    if ((normalized == QStringLiteral("false")) || (normalized == QStringLiteral("0")))
        return false;
    return kDefaultAllowRealLineBreaksInSnippets;
}


void writeAllowRealLineBreaksInSnippets(QSettings &settings, bool value) {
    settings.setValue(QString::fromLatin1(kAllowRealLineBreaksInSnippetsSettingKey), value);
}


} // namespace tlf
