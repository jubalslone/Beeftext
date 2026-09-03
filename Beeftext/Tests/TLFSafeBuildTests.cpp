/// \file
///
/// \brief Focused tests for the fail-closed TLF restricted-build parser.


#include "../TLFSafeBuild.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QSettings>
#include <QStringList>
#include <QTemporaryDir>


namespace {


int failureCount = 0;


void expect(bool condition, QString const &description) {
    if (condition)
        return;
    qCritical().noquote() << "FAILED:" << description;
    ++failureCount;
}


void expectText(QString const &actual, QString const &expected, QString const &description) {
    if (actual == expected)
        return;
    qCritical().noquote() << "FAILED:" << description << "expected" << expected << "but got" << actual;
    ++failureCount;
}


void testVariableAllowlist() {
    using Variable = tlf::ERestrictedVariable;
    expect(tlf::classifyVariable("cursor") == Variable::Cursor, "cursor is allowed");
    expect(tlf::classifyVariable("date") == Variable::Date, "date is allowed");
    expect(tlf::classifyVariable("time") == Variable::Time, "time is allowed");
    expect(tlf::classifyVariable("dateTime") == Variable::DateTime, "dateTime is allowed");
    expect(tlf::classifyVariable("dateTime:+1d:yyyy-MM-dd") == Variable::CustomDateTime,
           "valid shifted dateTime is allowed");
    expect(tlf::classifyVariable("dateTime:yyyy-MM-dd") == Variable::CustomDateTime,
           "valid unshifted dateTime is allowed");
    expect(tlf::classifyVariable("combo:client") == Variable::Combo, "combo is allowed");
    expect(tlf::classifyVariable("upper:client") == Variable::Upper, "upper is allowed");
    expect(tlf::classifyVariable("lower:client") == Variable::Lower, "lower is allowed");
    expect(tlf::classifyVariable("trim:client") == Variable::Trim, "trim is allowed");
    expect(tlf::classifyVariable("input:Client name") == Variable::Input, "input is allowed");

    QStringList const blocked = {
        "clipboard", "discordemoji", "envVar:USERNAME", "powershell:C:\\test.ps1",
        "key:enter", "shortcut:Win+R", "delay:50", "dateTime:+1q:yyyy", "unknown"
    };
    for (QString const &variable: blocked)
        expect(tlf::classifyVariable(variable) == Variable::Blocked,
               QString("%1 is blocked").arg(variable));
}


void testSanitizer() {
    expectText(tlf::sanitizeText("a\r\nb\nc\rd\te"), "a\\nb\\nc\\nd\\te",
               "line endings and tabs become visible text");

    QString controls;
    controls += QChar(0x0000);
    controls += QChar(0x0008);
    controls += QChar(0x001b);
    controls += QChar(0x007f);
    controls += QChar(0x0085);
    controls += QChar(0x2028);
    controls += QChar(0x2029);
    expectText(tlf::sanitizeText(controls),
               "\\u0000\\u0008\\u001B\\u007F\\u0085\\u2028\\u2029",
               "control and Unicode separator characters become visible text");

    QString emoji;
    emoji += QChar(0xd83d);
    emoji += QChar(0xde00);
    expectText(tlf::sanitizeText(emoji), emoji, "valid UTF-16 surrogate pairs are preserved");

    QString unpaired;
    unpaired += QChar(0xd83d);
    unpaired += 'x';
    unpaired += QChar(0xde00);
    expectText(tlf::sanitizeText(unpaired), "\\uD83Dx\\uDE00", "unpaired surrogates become visible text");
    expectText(tlf::sanitizeText(tlf::sanitizeText(controls)), tlf::sanitizeText(controls),
               "sanitization is idempotent");
}


void testMultilineSanitizer() {
    QString const mixedLineEndings = "a\n\nb\r\nc\rd\n";
    expectText(tlf::sanitizeText(mixedLineEndings, true), "a\n\nb\nc\nd\n",
               "real-line-break mode normalizes LF, CRLF, and CR to one LF");
    expectText(tlf::sanitizeText(mixedLineEndings, false), "a\\n\\nb\\nc\\nd\\n",
               "strict mode renders every logical line break as visible text");
    expectText(tlf::sanitizeText(tlf::sanitizeText(mixedLineEndings, true), true),
               tlf::sanitizeText(mixedLineEndings, true),
               "real-line-break sanitization is idempotent");

    QString emoji;
    emoji += QChar(0xd83d);
    emoji += QChar(0xde00);
    QString const multilineEmoji = QString("before\n") + emoji + "\nafter";
    expectText(tlf::sanitizeText(multilineEmoji, true), multilineEmoji,
               "real line breaks do not change valid surrogate pairs");
    expectText(tlf::sanitizeText(QString(QChar(0x2028)), true), "\\u2028",
               "Unicode line separators remain visible in real-line-break mode");
}


void testBlockedControlsInBothModes() {
    QStringList const blocked = {
        "#{key:enter}", "#{shortcut:Win+R}", "#{delay:500}", "#{clipboard}",
        "#{envVar:USERNAME}", "#{powershell:C:\\test.ps1}"
    };
    for (bool const allowRealLineBreaks: { false, true }) {
        for (QString const &token: blocked) {
            tlf::RestrictedSnippet const snippet = tlf::prepareSnippet(token, allowRealLineBreaks);
            expectText(snippet.text, token,
                QString("%1 remains literal when real line breaks are %2")
                    .arg(token, allowRealLineBreaks ? "allowed" : "visible"));
            expect(snippet.cursorLeftCount == -1, "blocked syntax cannot create cursor movement");
        }
    }

    tlf::RestrictedSnippet const multiline =
        tlf::prepareSnippet("first\n#{key:enter}\nlast", true);
    expectText(multiline.text, "first\n#{key:enter}\nlast",
               "allowed text line breaks do not activate a blocked Enter variable");
}


void testCursorPlan() {
    tlf::RestrictedSnippet snippet = tlf::prepareSnippet("plain");
    expectText(snippet.text, "plain", "plain text is unchanged");
    expect(snippet.cursorLeftCount == -1, "plain text has no cursor action");

    snippet = tlf::prepareSnippet("#{cursor}abc");
    expectText(snippet.text, "abc", "cursor marker at start is removed");
    expect(snippet.cursorLeftCount == 3, "cursor marker at start moves within inserted text");

    snippet = tlf::prepareSnippet("ab#{cursor}cd");
    expectText(snippet.text, "abcd", "cursor marker in middle is removed");
    expect(snippet.cursorLeftCount == 2, "cursor marker in middle has a bounded move");

    snippet = tlf::prepareSnippet("abc#{cursor}");
    expectText(snippet.text, "abc", "cursor marker at end is removed");
    expect(snippet.cursorLeftCount == 0, "cursor marker at end needs no move");

    snippet = tlf::prepareSnippet("#{cursor}X#{CURSOR}");
    expectText(snippet.text, "X#{CURSOR}", "mixed-case cursor text remains literal");
    expect(snippet.cursorLeftCount == snippet.text.size(), "mixed-case literal is included in the cursor bound");

    QString const repeated = "a#{cursor}b#{cursor}c";
    snippet = tlf::prepareSnippet(repeated);
    expectText(snippet.text, repeated, "multiple exact cursor markers remain literal");
    expect(snippet.cursorSyntaxRejected, "multiple exact cursor markers are rejected");

    QString const unicodeSuffix = "a#{cursor}\u00e9";
    snippet = tlf::prepareSnippet(unicodeSuffix);
    expectText(snippet.text, unicodeSuffix, "Unicode cursor suffix remains literal");
    expect(snippet.cursorSyntaxRejected, "Unicode cursor suffix is rejected as an ambiguous move");

    snippet = tlf::prepareSnippet("a#{cursor}#{key:enter}#{delay:10}");
    expectText(snippet.text, "a#{key:enter}#{delay:10}", "blocked control syntax remains visible");
    expect(snippet.cursorLeftCount == QString("#{key:enter}#{delay:10}").size(),
           "blocked literal syntax is included in the cursor bound");

    snippet = tlf::prepareSnippet("before\n#{cursor}after\nline", true);
    expectText(snippet.text, "before\nafter\nline", "newlines before and after cursor are preserved when allowed");
    expect(snippet.cursorLeftCount == QString("after\nline").size(),
           "each allowed logical line break counts as one bounded cursor movement");

    snippet = tlf::prepareSnippet("before#{cursor}\r\n", true);
    expectText(snippet.text, "before\n", "CRLF after cursor is normalized to one line break");
    expect(snippet.cursorLeftCount == 1, "normalized CRLF requires one bounded cursor movement");
}


void testMultilinePreferencePersistence() {
    QTemporaryDir temporaryDirectory;
    expect(temporaryDirectory.isValid(), "temporary preference directory is available");
    if (!temporaryDirectory.isValid())
        return;

    QString const dataFolder = QDir(temporaryDirectory.path()).absoluteFilePath("Data");
    expect(QDir().mkpath(dataFolder), "portable Data test folder is created");
    QString const settingsPath = QDir(dataFolder).absoluteFilePath("Settings.ini");

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        expect(!tlf::readAllowRealLineBreaksInSnippets(settings),
               "missing multiline preference defaults to visible line breaks");
        tlf::writeAllowRealLineBreaksInSnippets(settings, true);
        settings.sync();
        expect(settings.status() == QSettings::NoError, "multiline preference is written without error");
    }

    expect(QFileInfo(settingsPath).exists(), "portable multiline preference is stored under Data");
    {
        QSettings reloaded(settingsPath, QSettings::IniFormat);
        expect(tlf::readAllowRealLineBreaksInSnippets(reloaded),
               "multiline preference survives a settings reload");

        QJsonObject exported;
        tlf::exportAllowRealLineBreaksInSnippets(reloaded, exported);
        expect(exported.value(QString::fromLatin1(tlf::kAllowRealLineBreaksInSnippetsSettingKey)).toBool(),
               "multiline preference is included in preference export");

        tlf::writeAllowRealLineBreaksInSnippets(reloaded, false);
        tlf::importAllowRealLineBreaksInSnippets(exported, reloaded);
        expect(tlf::readAllowRealLineBreaksInSnippets(reloaded),
               "multiline preference is restored from preference import");

        tlf::importAllowRealLineBreaksInSnippets(QJsonObject(), reloaded);
        expect(!tlf::readAllowRealLineBreaksInSnippets(reloaded),
               "older preference imports without the setting remain fail-closed");

        reloaded.setValue(QString::fromLatin1(tlf::kAllowRealLineBreaksInSnippetsSettingKey),
                          "not-a-boolean");
        expect(!tlf::readAllowRealLineBreaksInSnippets(reloaded),
               "invalid stored multiline preference remains fail-closed");
    }
}


} // anonymous namespace


int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    testVariableAllowlist();
    testSanitizer();
    testMultilineSanitizer();
    testBlockedControlsInBothModes();
    testCursorPlan();
    testMultilinePreferencePersistence();
    if (failureCount == 0)
        qInfo() << "All TLF restricted-build tests passed.";
    return failureCount == 0 ? 0 : 1;
}
