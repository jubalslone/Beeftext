/// \file
///
/// \brief Focused tests for the fail-closed TLF restricted-build parser.


#include "../TLFSafeBuild.h"
#include "../Combo/ComboPortability.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>


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

    snippet = tlf::prepareSnippet("parent child-#{cursor}end");
    expectText(snippet.text, "parent child-end",
               "one cursor from a nested-combo expansion is removed");
    expect(snippet.cursorLeftCount == QString("end").size(),
           "one post-expansion nested cursor has a bounded move");

    snippet = tlf::prepareSnippet("#{cursor}X#{CURSOR}");
    expectText(snippet.text, "X#{CURSOR}", "mixed-case cursor text remains literal");
    expect(snippet.cursorLeftCount == snippet.text.size(), "mixed-case literal is included in the cursor bound");

    QString const repeated = "a#{cursor}b#{cursor}c";
    snippet = tlf::prepareSnippet(repeated);
    expectText(snippet.text, "abc", "two direct cursor markers are removed");
    expect(snippet.cursorLeftCount == 1, "the last direct cursor marker wins");
    expect(!snippet.cursorSyntaxRejected, "two safe direct cursor markers are deterministic");

    QString const twiceExpandedChild = "parent child-#{cursor}end / child-#{cursor}end";
    snippet = tlf::prepareSnippet(twiceExpandedChild);
    expectText(snippet.text, "parent child-end / child-end",
               "cursor markers from two nested-combo expansions are removed");
    expect(snippet.cursorLeftCount == QString("end").size(),
           "the last cursor from repeated nested-combo expansion wins");

    snippet = tlf::prepareSnippet("parent #{cursor}one / child-#{cursor}two");
    expectText(snippet.text, "parent one / child-two",
               "direct and nested cursor markers share one post-expansion rule");
    expect(snippet.cursorLeftCount == QString("two").size(),
           "the last post-expansion marker wins regardless of its origin");

    snippet = tlf::prepareSnippet("UPPER-#{CURSOR}-TEXT");
    expectText(snippet.text, "UPPER-#{CURSOR}-TEXT",
               "an upper transformation makes a cursor marker literal");
    expect(snippet.cursorLeftCount == -1,
           "transformed mixed-case cursor text cannot create movement");

    snippet = tlf::prepareSnippet("lower-#{cursor}-text");
    expectText(snippet.text, "lower--text",
               "a lower or trim transformation that preserves an exact marker remains usable");
    expect(snippet.cursorLeftCount == QString("-text").size(),
           "transformation output uses the same bounded suffix rule");

    QString const unicodeSuffix = "a#{cursor}\u00e9";
    snippet = tlf::prepareSnippet(unicodeSuffix);
    expectText(snippet.text, unicodeSuffix, "Unicode cursor suffix remains literal");
    expect(snippet.cursorSyntaxRejected, "Unicode cursor suffix is rejected as an unsafe move");

    QString const repeatedUnsafe = "a#{cursor}b#{cursor}\u00e9";
    snippet = tlf::prepareSnippet(repeatedUnsafe);
    expectText(snippet.text, repeatedUnsafe,
               "all markers remain literal when the final cursor suffix is unsafe");
    expect(snippet.cursorLeftCount == -1,
           "unsafe repeated cursor syntax cannot create movement");
    expect(snippet.cursorSyntaxRejected,
           "unsafe repeated cursor syntax fails closed");

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

    snippet = tlf::prepareSnippet("first#{cursor}\nsecond#{cursor}\nthird", true);
    expectText(snippet.text, "first\nsecond\nthird",
               "multiple markers preserve real line breaks after expansion");
    expect(snippet.cursorLeftCount == QString("\nthird").size(),
           "real-line-break mode bounds movement from the last marker");

    snippet = tlf::prepareSnippet("first#{cursor}\r\nsecond#{cursor}\r\nthird", false);
    expectText(snippet.text, "first\\nsecond\\nthird",
               "multiple markers preserve visible line breaks in strict mode");
    expect(snippet.cursorLeftCount == QString("\\nthird").size(),
           "strict mode bounds movement from the last marker");
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

        reloaded.setValue(QString::fromLatin1(tlf::kAllowRealLineBreaksInSnippetsSettingKey),
                          "not-a-boolean");
        expect(!tlf::readAllowRealLineBreaksInSnippets(reloaded),
               "invalid stored multiline preference remains fail-closed");
    }
}


QJsonObject testGroup(QString const &uuid, QString const &name) {
	return QJsonObject {
		{ "uuid", uuid },
		{ "name", name },
		{ "description", QString("Test group") },
		{ "creationDateTime", QString("2026-09-05T12:00:00.000") },
		{ "modificationDateTime", QString("2026-09-05T12:00:00.000") },
		{ "enabled", true },
	};
}


QJsonObject testCombo(QString const &uuid, QString const &keyword, QString const &groupUuid) {
	return QJsonObject {
		{ "uuid", uuid },
		{ "name", keyword },
		{ "keyword", keyword },
		{ "snippet", QString("Snippet for %1").arg(keyword) },
		{ "description", QString("Test combo") },
		{ "matchingMode", 0 },
		{ "caseSensitivity", 0 },
		{ "group", groupUuid },
		{ "creationDateTime", QString("2026-09-05T12:00:00.000") },
		{ "modificationDateTime", QString("2026-09-05T12:00:00.000") },
		{ "enabled", true },
	};
}


bool writeTestFile(QString const &path, QByteArray const &data) {
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly))
		return false;
	return file.write(data) == data.size();
}


void testComboExportBundle() {
	QString const groupAUuid = QUuid::createUuid().toString();
	QString const groupBUuid = QUuid::createUuid().toString();
	QJsonArray const groups = {
		testGroup(groupAUuid, "Group A"),
		testGroup(groupBUuid, "Group B"),
	};
	QJsonObject const comboOne = testCombo(QUuid::createUuid().toString(), "one", groupAUuid);
	QJsonObject const comboTwo = testCombo(QUuid::createUuid().toString(), "two", groupAUuid);
	QJsonObject const comboThree = testCombo(QUuid::createUuid().toString(), "three", groupBUuid);

	QJsonArray const oneSelected = { comboOne };
	QJsonDocument const oneBundle = combo_portability::createLeanBundle(oneSelected,
		combo_portability::referencedGroups(oneSelected, groups));
	expect(oneBundle.object().value("combos").toArray().size() == 1,
		"one selected combo is exported through the shared bundle path");
	expect(oneBundle.object().value("groups").toArray().size() == 1,
		"one selected combo carries its group");

	QJsonArray const multipleSelected = { comboOne, comboThree };
	QJsonDocument const multipleBundle = combo_portability::createLeanBundle(multipleSelected,
		combo_portability::referencedGroups(multipleSelected, groups));
	expect(multipleBundle.object().value("combos").toArray().size() == 2,
		"multiple selected combos are exported through the shared bundle path");
	expect(multipleBundle.object().value("groups").toArray().size() == 2,
		"multiple selected combos carry every referenced group");

	QJsonArray const allCombos = { comboOne, comboTwo, comboThree };
	QJsonDocument const allBundle = combo_portability::createLeanBundle(allCombos, groups);
	expect(allBundle.object().value("combos").toArray().size() == 3,
		"all combos are exported through the shared bundle path");
	expect(allBundle.object().value("groups").toArray() == groups,
		"an all-combos export preserves the complete group list, including empty groups");
	expectText(allBundle.object().value("format").toString(), "lean-beeftext-combos",
		"Lean combo bundle is self-identifying");
	expect(allBundle.object().value("version").toInt() == 1,
		"Lean combo bundle has schema version 1");
	QSet<QString> const keys = { "format", "version", "groups", "combos" };
	QSet<QString> actualKeys;
	for (QString const &key: allBundle.object().keys())
		actualKeys.insert(key);
	expect(actualKeys == keys, "Lean combo bundle contains no preferences or settings data");
}


void testComboPortabilityFiles() {
	QTemporaryDir directory;
	expect(directory.isValid(), "temporary combo-portability directory is available");
	if (!directory.isValid())
		return;

	QString const groupUuid = QUuid::createUuid().toString();
	QJsonArray const groups = { testGroup(groupUuid, "Clients") };
	QJsonArray const combos = { testCombo(QUuid::createUuid().toString(), "client", groupUuid) };
	QString const leanPath = QDir(directory.path()).absoluteFilePath("Lean-Beeftext-Combos.txt");
	QString error;
	expect(combo_portability::saveLeanBundle(leanPath, combos, groups, &error),
		QString("Lean .txt export succeeds: %1").arg(error));
	QFile leanFile(leanPath);
	expect(leanFile.open(QIODevice::ReadOnly), "Lean .txt export can be reopened");
	QJsonParseError parseError;
	QJsonDocument const exportedDocument = QJsonDocument::fromJson(leanFile.readAll(), &parseError);
	expect(parseError.error == QJsonParseError::NoError && exportedDocument.isObject(),
		"Lean .txt export contains valid UTF-8 JSON");

	QJsonDocument importedDocument;
	bool preserveGroups = false;
	error.clear();
	expect(combo_portability::loadJsonForImport(leanPath, 10, importedDocument, preserveGroups, &error),
		QString("Lean .txt import succeeds: %1").arg(error));
	expect(preserveGroups, "Lean .txt import requests group preservation");
	expect(importedDocument.object().value("combos").toArray() == combos,
		"Lean .txt round trip preserves combo records");
	expect(importedDocument.object().value("groups").toArray() == groups,
		"Lean .txt round trip preserves group records and relationships");

	QJsonObject legacyRoot;
	legacyRoot.insert("fileFormatVersion", 10);
	legacyRoot.insert("groups", QJsonArray());
	legacyRoot.insert("combos", combos);
	QString const legacyJsonPath = QDir(directory.path()).absoluteFilePath("upstream.json");
	expect(writeTestFile(legacyJsonPath, QJsonDocument(legacyRoot).toJson()),
		"legacy upstream JSON fixture is written");
	preserveGroups = true;
	expect(combo_portability::loadJsonForImport(legacyJsonPath, 10, importedDocument, preserveGroups, &error),
		"legacy upstream JSON remains accepted");
	expect(!preserveGroups, "legacy upstream JSON keeps destination-group import behavior");

	QString const legacyCsvPath = QDir(directory.path()).absoluteFilePath("upstream.csv");
	expect(writeTestFile(legacyCsvPath, "legacy,Legacy snippet,Legacy name\n"),
		"legacy upstream CSV fixture is written");
	QVector<QStringList> rows;
	expect(combo_portability::loadLegacyCsvRows(legacyCsvPath, rows, &error),
		"legacy upstream CSV remains accepted");
	expect(rows.size() == 1 && rows[0] == QStringList({ "legacy", "Legacy snippet", "Legacy name" }),
		"legacy upstream CSV fields retain their compatible meaning");

	QJsonDocument const sentinel(QJsonObject { { "sentinel", true } });
	importedDocument = sentinel;
	preserveGroups = true;
	QString const malformedPath = QDir(directory.path()).absoluteFilePath("malformed.txt");
	expect(writeTestFile(malformedPath, "{ definitely-not-json"), "malformed Lean fixture is written");
	expect(!combo_portability::loadJsonForImport(malformedPath, 10, importedDocument, preserveGroups, &error),
		"malformed Lean .txt fails clearly");
	expect(!error.isEmpty(), "malformed Lean .txt provides an error message");
	expect(importedDocument == sentinel && preserveGroups,
		"malformed Lean .txt does not partially change import output");

	QString const unsupportedPath = QDir(directory.path()).absoluteFilePath("old.btbackup");
	expect(writeTestFile(unsupportedPath, "ignored"), "unsupported fixture is written");
	error.clear();
	expect(!combo_portability::loadJsonForImport(unsupportedPath, 10, importedDocument, preserveGroups, &error),
		"unsupported files are rejected");
	expect(!error.isEmpty() && importedDocument == sentinel && preserveGroups,
		"unsupported files fail without partial import output");
}


QString readSourceFile(QString const &relativePath) {
	QFile file(QDir(QStringLiteral(BEEFTEXT_SOURCE_DIR)).absoluteFilePath(relativePath));
	if (!file.open(QIODevice::ReadOnly)) {
		expect(false, QString("UI source can be read: %1").arg(relativePath));
		return QString();
	}
	return QString::fromUtf8(file.readAll());
}


void testRestrictedPortabilityUiSurface() {
	QString const mainWindowUi = readSourceFile("MainWindow.ui");
	expect(!mainWindowUi.contains("actionBackup") && !mainWindowUi.contains("actionRestore")
		&& !mainWindowUi.contains("Back Up Combos") && !mainWindowUi.contains("Restore Combos"),
		"File menu exposes no backup or restore actions");

	QString const advancedUi = readSourceFile("Preferences/Panes/PrefPaneAdvanced.ui");
	expect(!advancedUi.contains("Automatic combo backup") && !advancedUi.contains("Restore Combo Backup")
		&& !advancedUi.contains("checkAutoBackup"),
		"Advanced Preferences exposes no automatic-backup workflow");

	QString const tableSource = readSourceFile("Combo/ComboTableWidget.cpp");
	expect(tableSource.contains("&Import Combos…") && tableSource.contains("&Export Combos…"),
		"Combos portability surface exposes Import Combos and Export Combos");
	expect(tableSource.contains("QMenu *ComboTableWidget::portabilityMenu")
		&& readSourceFile("MainWindow.cpp").contains("portabilityMenu(this)"),
		"top-level Combos menu uses the dedicated two-command portability surface");
	expect(!tableSource.contains("Export All Combos") && !tableSource.contains("Export Selected Combo")
		&& !tableSource.contains("actionExportAllCombos_"),
		"separate selected/all export actions are absent");
	expect(!readSourceFile("Preferences/PreferencesDialog.ui").contains("Export Preferences")
		&& !readSourceFile("Preferences/PreferencesDialog.ui").contains("Import Preferences"),
		"Preferences export/import controls remain absent");
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
	testComboExportBundle();
	testComboPortabilityFiles();
	testRestrictedPortabilityUiSurface();
    if (failureCount == 0)
        qInfo() << "All TLF restricted-build tests passed.";
    return failureCount == 0 ? 0 : 1;
}
