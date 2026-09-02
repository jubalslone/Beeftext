/// \file
/// \author Xavier Michelon
///
/// \brief Implementation of functions related to combo variables


#include "stdafx.h"
#include "ComboVariable.h"
#include "ComboManager.h"
#include "Dialogs/VariableInputDialog.h"
#include "Preferences/PreferencesManager.h"
#include "Clipboard/ClipboardManager.h"
#include "BeeftextGlobals.h"
#include "BeeftextConstants.h"
#include <XMiLib/RandomNumberGenerator.h>
#include <XMiLib/Exception.h>
#include <limits>


namespace {


enum class ECaseChange {
    NoChange, ///< Do not change case
    ToUpper, ///< Convert to upper case
    ToLower ///< Conver to lower case
}; ///< Enumeration for case change


QString const kCustomDateTimeVariable = "dateTime:"; ///< The dateTime variable.
QString const kInputVariable = "input:"; ///< The input variable.
QString const kEnvVarVariable = "envVar:"; ///< The envVar variable.
QString const kPowershellVariable = "powershell:"; ///< The execute variable.


//****************************************************************************************************************************************************
/// \brief Resolve the escaped characters ( \\} and \\\\ in a variable parameter.
/// \param[in] paramStr The variable parameter.
/// \return The parameter where the escaped characters have been resolved.
//****************************************************************************************************************************************************
QString resolveEscapingInVariableParameter(QString paramStr) {
    paramStr.replace(R"(\\)", R"(\)");
    paramStr.replace(R"(\})", R"(})");
    return paramStr;
}


//****************************************************************************************************************************************************
/// \brief Converts a character to a Discord emoji. 
///
/// \note 'a space is added a the end of the returned emoji because Discord sometimes concatenates emoji. For instance
/// to letter emoji corresponding to a country code are concatenated into a flag emoji
///
/// \param[in] c The character
/// \return A string containing the emoji corresponding to the character
/// \return A string containing a series of whitespaces if no matching emoji exists
//****************************************************************************************************************************************************
QString qcharToDiscordEmoji(QChar const &c) {
    QChar const l = c.toLower();
    if ((l >= 'a') && (l <= 'z'))
        return QString(":regional_indicator_%1: ").arg(l);
    QMap<QChar, QString> const substs = {{ '0', ":zero: " },
                                         { '1', ":one: " },
                                         { '2', ":two: " },
                                         { '3', ":three: " },
                                         { '4', ":four: " },
                                         { '5', ":five: " },
                                         { '6', ":six: " },
                                         { '7', ":seven: " },
                                         { '8', ":eight: " },
                                         { '9', ":nine: " },
                                         { '!', ":exclamation: " },
                                         { '?', ":question: " }};
    return substs.value(c, "        ");
}


//****************************************************************************************************************************************************
/// \brief Create a Discord emoji representation of the content of the clipboard
///
/// \return A string containing the sequence of Discord emojis
//****************************************************************************************************************************************************
QString discordEmojisFromClipboard() {
    QString str = ClipboardManager::instance().text();
    QString result;
    for (QChar const &c: str)
        result += qcharToDiscordEmoji(c);
    return result;
}


//****************************************************************************************************************************************************
/// \brief Split a timeshift string (e.g. +1d-4w+11h), into individual shifts (e.g. { +1d, -4w, +11h)
///
/// \param[in] shiftStr The timeshift string
/// \return a string list containing the individual shifts
//****************************************************************************************************************************************************
QStringList splitTimeShiftString(QString const &shiftStr) {
    QStringList result;
    QString acc;
    for (QChar const c: shiftStr) {
        if (('+' == c) || ('-' == c)) {
            if (!acc.isEmpty()) {
                result.append(acc);
                acc = QString();
            }
        }
        acc += c;
    }
    if (!acc.isEmpty())
        result.append(acc);
    return result;
}


bool checkedScale(qint64 value, qint64 factor, qint64 &result) {
    Q_ASSERT(factor > 0);
    if ((value > (std::numeric_limits<qint64>::max() / factor))
        || (value < (std::numeric_limits<qint64>::min() / factor)))
        return false;
    result = value * factor;
    return true;
}


//****************************************************************************************************************************************************
/// \brief Returns the current date shifted according to the instructions in the shift string.
/// \param[in] shiftStr The string describing the timeshift (as defined in the dateTime: variable documentation.
/// \return The current date shifted according to the instructions in the shift string.
//****************************************************************************************************************************************************
QDateTime shiftedDateTime(QString const &shiftStr) {
    QDateTime result = QDateTime::currentDateTime();

    QStringList const shifts = splitTimeShiftString(shiftStr);
    for (QString const &shift: shifts) {
        QRegularExpressionMatch const match = QRegularExpression(R"(([+-])(\d+)([yMwdhmsz]))").match(shift);
        if (!match.hasMatch())
            return QDateTime();
        bool ok = false;
        qint64 value = match.captured(2).toLongLong(&ok);
        if (!ok)
            return QDateTime();
        if (match.captured(1) == "-")
            value = -value;
        qint64 scaledValue = 0;
        switch (match.captured(3)[0].toLatin1()) {
        case 'y':
            if ((value < std::numeric_limits<qint32>::min()) || (value > std::numeric_limits<qint32>::max()))
                return QDateTime();
            result = result.addYears(static_cast<qint32>(value));
            break;
        case 'M':
            if ((value < std::numeric_limits<qint32>::min()) || (value > std::numeric_limits<qint32>::max()))
                return QDateTime();
            result = result.addMonths(static_cast<qint32>(value));
            break;
        case 'w':
            if (!checkedScale(value, 7, scaledValue))
                return QDateTime();
            result = result.addDays(scaledValue);
            break;
        case 'd':
            result = result.addDays(value);
            break;
        case 'h':
            if (!checkedScale(value, 3600, scaledValue))
                return QDateTime();
            result = result.addSecs(scaledValue);
            break;
        case 'm':
            if (!checkedScale(value, 60, scaledValue))
                return QDateTime();
            result = result.addSecs(scaledValue);
            break;
        case 's':
            result = result.addSecs(value);
            break;
        case 'z':
            result = result.addMSecs(value);
            break;
        default:
            break;
        }
        if (!result.isValid())
            return QDateTime();
    }
    return result;
}


//****************************************************************************************************************************************************
/// \brief Evaluate a #[dateTime:} variable
/// \param[in] variable The variable.
/// \return the result of the evaluation.
//****************************************************************************************************************************************************
QString evaluateDateTimeVariable(QString const &variable) {
    QRegularExpression const regExp(R"(^dateTime(:(([+-]\d+[yMwdhmsz])+))?:(.*)$)");
    QRegularExpressionMatch const match = regExp.match(variable);
    if (!match.hasMatch())
        return QString();
    QDateTime const dateTime = match.captured(1).isEmpty() ? QDateTime::currentDateTime() :
                               shiftedDateTime(match.captured(2));
    QString formatStr = resolveEscapingInVariableParameter(match.captured(4));
    if (formatStr.isEmpty())
        return QLocale::system().toString(dateTime);

    qint32 const weekNumber = dateTime.date().weekNumber(); // we add support of ww and w for week number in format string.
    formatStr = formatStr.replace("ww", QString("%1").arg(weekNumber, 2, 10, QChar('0')));
    formatStr = formatStr.replace("w", QString::number(weekNumber));

    return QLocale::system().toString(dateTime, formatStr);
}


//****************************************************************************************************************************************************
/// \brief Evaluate a #{combo:} variable.
///
/// \param[in] variable The variable, without the enclosing #{}.
/// \param[in] caseChange The change of case (uppercase, lowercase) to apply to the evaluated variable.
/// \param[in] forbiddenSubCombos The text of the combos that are not allowed to be substituted using #{combo:}, to 
/// avoid endless recursion.
/// \param[in,out] knownInputVariables The list of know input variables.
/// \param[out] outCancelled Was the input variable cancelled by the user.
/// \return The result of evaluating the variable.
//****************************************************************************************************************************************************
QString evaluateComboVariable(QString const &variable, ECaseChange caseChange, QSet<QString> forbiddenSubCombos,
    QMap<QString, QString> &knownInputVariables, bool &outCancelled) {
    QString fallbackResult = QString("#{%1}").arg(variable);
    qint32 const varNameLength = qint32(variable.indexOf(':'));
    if (varNameLength < 0)
        return QString();
    QString const comboName = resolveEscapingInVariableParameter(variable.right(variable.size() - varNameLength - 1));
    if (forbiddenSubCombos.contains(comboName))
        return fallbackResult;

    ComboList results;
    for (SpCombo const &combo: ComboManager::instance().comboListRef())
        if (combo->keyword() == comboName)
            results.push_back(combo);

    qint32 const resultCount = results.size();
    ComboList::const_iterator it;
    switch (resultCount) {
    case 0:
        return fallbackResult;
    case 1:
        it = results.begin();
        break;
    default: {
        xmilib::RandomNumberGenerator rng(0, results.size() - 1);
        it = results.begin() + rng.get();
        break;
    }
    }

    QString str = (*it)->evaluatedSnippet(outCancelled, forbiddenSubCombos << comboName, knownInputVariables); // forbiddenSubcombos is intended at avoiding endless recursion
    switch (caseChange) {
    case ECaseChange::ToUpper:
        return str.toUpper();
    case ECaseChange::ToLower:
        return str.toLower();
    case ECaseChange::NoChange:
    default:
        return str;
    }
}


//****************************************************************************************************************************************************
/// \brief Evaluate an #{input:} variable.
///
/// \param[in] variable The variable, without the enclosing #{}.
/// \param[in,out] knownInputVariables The list of know input variables.
/// \param[out] outCancelled Was the input variable cancelled by the user.
/// \return The result of evaluating the variable.
//****************************************************************************************************************************************************
QString evaluateInputVariable(QString const &variable, QMap<QString, QString> &knownInputVariables, bool &outCancelled) {
    // check if we already add the user input for the given description
    QString const description = variable.right(variable.size() - kInputVariable.size());
    if (knownInputVariables.contains(description))
        return knownInputVariables[description];

    QString result;
    if (!VariableInputDialog::run(resolveEscapingInVariableParameter(description), result)) {
        outCancelled = true;
        return QString();
    }
    knownInputVariables.insert(description, result); // add the new input to the list of known ones
    return result;

}


//****************************************************************************************************************************************************
/// \brief Evaluate an #{envvar:} variable.
///
/// \param[in] variable The variable, without the enclosing #{}.
/// \return The result of evaluating the variable.
//****************************************************************************************************************************************************
QString evaluateEnvVarVariable(QString const &variable) {
    return QProcessEnvironment::systemEnvironment().value(variable.right(variable.size() -
                                                                         kEnvVarVariable.size()));
}


//****************************************************************************************************************************************************
/// \brief Evaluate an #{execute:} variable.
///
/// \param[in] variable The variable, without the enclosing #{}.
/// \return The result of evaluating the variable.
//****************************************************************************************************************************************************
QString evaluatePowershellVariable(QString const &variable) {
    try {
        QRegularExpression const rx(QString(R"(^%1(.+?)(?>:(\d+))?$)").arg(kPowershellVariable));
        QRegularExpressionMatch const match = rx.match(variable);
        if (!match.hasMatch())
            throw xmilib::Exception("An unexpected error occurred while parsing a powershell variable.");
        QString const path = match.captured(1);
        if (!QFileInfo(path).exists())
            throw xmilib::Exception(QString("the file `%1` does not exist.").arg(path));

        QString const timeoutStr = match.captured(2);
        bool ok = true;
        qint32 const timeout = timeoutStr.isEmpty() ? 10000 : timeoutStr.toInt(&ok);
        if (!ok)
            throw xmilib::Exception("An unexpected error occurred while parsing the delay of a PowerShell variable.");

        PreferencesManager const &prefs = PreferencesManager::instance();
        QString exePath = "powershell.exe";
        if (prefs.useCustomPowershellVersion()) {
            QString const customPath = prefs.customPowershellPath();
            QFileInfo const fi(customPath);
            if (fi.exists() && fi.isExecutable())
                exePath = customPath;
            else
                globals::debugLog().addWarning(QString("The custom PowerShell executable '%1' is invalid or not "
                                                       "executable.").arg(QDir::toNativeSeparators(customPath)));
        }

        QProcess p;
        p.start(exePath, { "-NonInteractive", "-ExecutionPolicy", "Unrestricted", "-File", path });
        if (!p.waitForFinished(timeout < 1 ? -1 : timeout))
            throw xmilib::Exception(QString("the script `%1` timed out.").arg(path));

        qint32 const returnCode = p.exitCode();
        if (returnCode)
            throw xmilib::Exception(QString("execution of `%1` return an error (code %2).").arg(path).arg(returnCode));
        return QString::fromUtf8(p.readAllStandardOutput());
    }
    catch (xmilib::Exception const &e) {
        globals::debugLog().addWarning(QString("Evaluation of #{%1} variable failed: %2").arg(kPowershellVariable)
            .arg(e.qwhat()));
        return QString();
    }
}


}


//****************************************************************************************************************************************************
/// \param[in] variable The variable, without the enclosing #{}.
/// \param[in] forbiddenSubCombos The text of the combos that are not allowed to be substituted using #{combo:}, to 
/// avoid endless recursion.
/// \param[in,out] knownInputVariables The list of know input variables.
/// \param[out] outCancelled Was the input variable cancelled by the user.
/// \return The result of evaluating the variable.
//****************************************************************************************************************************************************
QString evaluateVariable(QString const &variable, QSet<QString> const &forbiddenSubCombos,
    QMap<QString, QString> &knownInputVariables, bool &outCancelled) {
    outCancelled = false;
    QLocale const systemLocale = QLocale::system();

    if constexpr (constants::kRestrictedBuild) {
        QString const literal = QString("#{%1}").arg(variable);
        switch (tlf::classifyVariable(variable)) {
        case tlf::ERestrictedVariable::Cursor:
            return literal; // Resolved once, after the complete snippet has been evaluated.
        case tlf::ERestrictedVariable::Date:
            return systemLocale.toString(QDate::currentDate());
        case tlf::ERestrictedVariable::Time:
            return systemLocale.toString(QTime::currentTime());
        case tlf::ERestrictedVariable::DateTime:
            return systemLocale.toString(QDateTime::currentDateTime());
        case tlf::ERestrictedVariable::CustomDateTime:
        {
            QString const value = evaluateDateTimeVariable(variable);
            return value.isEmpty() ? literal : value;
        }
        case tlf::ERestrictedVariable::Combo:
            return evaluateComboVariable(variable, ECaseChange::NoChange, forbiddenSubCombos,
                                         knownInputVariables, outCancelled);
        case tlf::ERestrictedVariable::Upper:
            return evaluateComboVariable(variable, ECaseChange::ToUpper, forbiddenSubCombos,
                                         knownInputVariables, outCancelled);
        case tlf::ERestrictedVariable::Lower:
            return evaluateComboVariable(variable, ECaseChange::ToLower, forbiddenSubCombos,
                                         knownInputVariables, outCancelled);
        case tlf::ERestrictedVariable::Trim:
            return evaluateComboVariable(variable, ECaseChange::NoChange, forbiddenSubCombos,
                                         knownInputVariables, outCancelled).trimmed();
        case tlf::ERestrictedVariable::Input:
            return evaluateInputVariable(variable, knownInputVariables, outCancelled);
        case tlf::ERestrictedVariable::Blocked:
        default:
            globals::debugLog().addWarning("Blocked a restricted combo variable.");
            return literal;
        }
    }

    if (variable == "clipboard")
        return ClipboardManager::instance().text();

    if (variable == "discordemoji") //secret variable that create text in Discord emoji from the clipboard text
        return discordEmojisFromClipboard();

    if (variable == "date")
        return systemLocale.toString(QDate::currentDate());

    if (variable == "time")
        return systemLocale.toString(QTime::currentTime());

    if (variable == "dateTime")
        return systemLocale.toString(QDateTime::currentDateTime());

    if (variable.startsWith(kCustomDateTimeVariable))
        return evaluateDateTimeVariable(variable);

    if (variable.startsWith("combo:"))
        return evaluateComboVariable(variable, ECaseChange::NoChange, forbiddenSubCombos, knownInputVariables, outCancelled);

    if (variable.startsWith("upper:"))
        return evaluateComboVariable(variable, ECaseChange::ToUpper, forbiddenSubCombos, knownInputVariables, outCancelled);

    if (variable.startsWith("lower:"))
        return evaluateComboVariable(variable, ECaseChange::ToLower, forbiddenSubCombos, knownInputVariables, outCancelled);

    if (variable.startsWith("trim:")) {
        QString const var = evaluateComboVariable(variable, ECaseChange::NoChange, forbiddenSubCombos, knownInputVariables, outCancelled);
        return var.trimmed();
    }

    if (variable.startsWith(kInputVariable))
        return evaluateInputVariable(variable, knownInputVariables, outCancelled);

    if (variable.startsWith(kEnvVarVariable))
        return evaluateEnvVarVariable(variable);

    if (variable.startsWith(kPowershellVariable))
        return evaluatePowershellVariable(variable);

    return QString("#{%1}").arg(variable); // we could not recognize the variable, so we put it back in the result
}
