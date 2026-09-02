/// \file
///
/// \brief Focused tests for the fail-closed TLF restricted-build parser.


#include "../TLFSafeBuild.h"

#include <QCoreApplication>
#include <QDebug>
#include <QStringList>


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
}


} // anonymous namespace


int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    testVariableAllowlist();
    testSanitizer();
    testCursorPlan();
    if (failureCount == 0)
        qInfo() << "All TLF restricted-build tests passed.";
    return failureCount == 0 ? 0 : 1;
}
