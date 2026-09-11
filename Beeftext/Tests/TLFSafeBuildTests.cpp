/// \file
///
/// \brief Focused tests for the Lean Beeftext restricted execution model.


#include "../TLFSafeBuild.h"
#include "../Combo/ComboPortability.h"
#include "../Migration/LegacyMigrationCore.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <QUuid>

#include <cstdio>


namespace {


int failureCount = 0;


void expect(bool condition, QString const &description) {
    if (condition)
        return;
	QByteArray const utf8Description = description.toUtf8();
	std::fprintf(stderr, "FAILED: %s\n", utf8Description.constData());
    qCritical().noquote() << "FAILED:" << description;
    ++failureCount;
}


void expectText(QString const &actual, QString const &expected, QString const &description) {
    if (actual == expected)
        return;
	QByteArray const utf8Description = description.toUtf8();
	QByteArray const utf8Expected = expected.toUtf8();
	QByteArray const utf8Actual = actual.toUtf8();
	std::fprintf(stderr, "FAILED: %s expected %s but got %s\n", utf8Description.constData(),
		utf8Expected.constData(), utf8Actual.constData());
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


QString readRepositoryFile(QString const &relativePath) {
	QFile file(QDir(QStringLiteral(BEEFTEXT_SOURCE_DIR)).absoluteFilePath("../" + relativePath));
	if (!file.open(QIODevice::ReadOnly)) {
		expect(false, QString("repository file can be read: %1").arg(relativePath));
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
	QString const mainWindowSource = readSourceFile("MainWindow.cpp");
	expect(!tableSource.contains("portabilityMenu")
		&& mainWindowSource.contains("comboTableWidget()->menu(this)"),
		"top-level Combos menu uses the complete combo-management menu");
	expect(tableSource.contains("menu->addAction(actionNewCombo_)")
		&& tableSource.contains("menu->addAction(actionEditCombo_)")
		&& tableSource.contains("menu->addAction(actionDuplicateCombo_)")
		&& tableSource.contains("menu->addAction(actionDeleteCombo_)")
		&& tableSource.contains("menu->addAction(actionCopySnippet_)")
		&& tableSource.contains("menu->addAction(actionEnableDisableCombo_)")
		&& tableSource.contains("menu->addAction(actionSelectAll_)")
		&& tableSource.contains("menu->addAction(actionDeselectAll_)"),
		"normal combo-management actions remain on the shared Combos menu");
	qint32 const deselectPosition = tableSource.indexOf("menu->addAction(actionDeselectAll_)");
	qint32 const importPosition = tableSource.indexOf("menu->addAction(actionImportCombos_)");
	qint32 const exportPosition = tableSource.indexOf("menu->addAction(actionExportCombos_)");
	expect(deselectPosition >= 0 && importPosition > deselectPosition && exportPosition > importPosition
		&& tableSource.count("menu->addAction(actionImportCombos_)") == 1
		&& tableSource.count("menu->addAction(actionExportCombos_)") == 1,
		"one Import and one Export workflow appear at the bottom of the complete Combos menu");
	expect(tableSource.contains("buttonCombos->setMenu(this->menu(this))")
		&& readSourceFile("Combo/ComboTableWidget.ui").contains("name=\"buttonCombos\""),
		"the in-window Combos button remains visible and uses the complete shared menu");
	expect(combo_portability::importFileDialogFilter().startsWith(
		"Supported combo files (*.txt *.json *.csv);;Lean Beeftext combo files (*.txt);;"
		"Legacy Beeftext JSON files (*.json);;Legacy Beeftext CSV files (*.csv);;All files (*.*)"),
		"the import picker defaults to all supported combo formats");
	expect(!mainWindowSource.contains("setDefaultAction("),
		"the tray Open action is rendered as an ordinary menu action");
	expect(!tableSource.contains("Export All Combos") && !tableSource.contains("Export Selected Combo")
		&& !tableSource.contains("actionExportAllCombos_"),
		"separate selected/all export actions are absent");
	expect(!readSourceFile("Preferences/PreferencesDialog.ui").contains("Export Preferences")
		&& !readSourceFile("Preferences/PreferencesDialog.ui").contains("Import Preferences"),
		"Preferences export/import controls remain absent");
}


void testProductFinishingSurface() {
	QString const constantsHeader = readSourceFile("BeeftextConstants.h");
	QString const constantsSource = readSourceFile("BeeftextConstants.cpp");
	expect(constantsSource.contains("kApplicationName = \"Lean Beeftext\"")
		&& constantsSource.contains("kProductVersion = \"1.0.0\"")
		&& constantsSource.contains("kUpstreamVersion = \"16.0\""),
		"public product identity is Lean Beeftext 1.0.0 based on Beeftext 16.0");
	QRegularExpression const singleInstancePattern(
		R"regex(kSingleInstanceIdentifier\s*=\s*"([^"]+)")regex");
	QRegularExpressionMatch const singleInstanceMatch = singleInstancePattern.match(constantsSource);
	QString const singleInstanceIdentifier = singleInstanceMatch.captured(1);
	expect(singleInstanceMatch.hasMatch()
		&& singleInstanceIdentifier == "LeanBeeftextSingleInstanceIdentifier"
		&& !singleInstanceIdentifier.contains(QRegularExpression(R"(\d)"))
		&& constantsHeader.contains("kSingleInstanceIdentifier"),
		"Lean uses a named, stable, version-free single-instance identity");
	expect(constantsSource.contains("kVersionNumber(16, 0)")
		&& !readSourceFile("Dialogs/AboutDialog.cpp").contains("kVersionNumber")
		&& readRepositoryFile("CMakeLists.txt").contains("VERSION 1.0.0")
		&& readSourceFile("CMakeLists.txt").contains("VERSION 1.0.0")
		&& readSourceFile("Beeftext.rc").contains("VERSION_STRING \"1.0.0\\0\""),
		"public metadata is 1.0.0 while the disabled updater keeps its two-part upstream compatibility value");
    expect(constantsSource.contains("kSettingsApplicationName = \"Lean Beeftext\"")
        && constantsSource.contains("kOrganizationName = \"Jubal Slone\"")
        && constantsHeader.contains("kSettingsApplicationName")
        && readRepositoryFile("Installer/LeanBeeftext.iss").contains("#define MyAppPublisher \"Jubal Slone\""),
        "installed settings identity and public publisher attribution retain their permanent Lean values");
	expect(constantsSource.contains("https://github.com/jubalslone/lean-beeftext#variables"),
		"About Variables uses the README Variables anchor");

	QString const preferencesSource = readSourceFile("Preferences/PreferencesManager.cpp");
	QString const preferencesHeader = readSourceFile("Preferences/PreferencesManager.h");
	QString const appearanceUi = readSourceFile("Preferences/Panes/PrefPaneAppearance.ui");
	QString const preferencesDialogUi = readSourceFile("Preferences/PreferencesDialog.ui");
	QString const themeSource = readSourceFile("Theme.cpp");
	QString const pickerDelegateSource = readSourceFile("Picker/PickerItemDelegate.cpp");
	expect(!appearanceUi.contains("Override Windows theme")
		&& !appearanceUi.contains("Use custom theme")
		&& !appearanceUi.contains("checkUseCustomTheme")
		&& !appearanceUi.contains("comboTheme")
		&& !appearanceUi.contains(">Light<")
		&& !appearanceUi.contains(">Dark<"),
		"Preferences exposes no custom Light or Dark theme controls");
	expect(preferencesDialogUi.contains("<string>Language</string>")
		&& preferencesDialogUi.contains("paneAppearance")
		&& appearanceUi.contains("comboLocale")
		&& appearanceUi.contains("buttonTranslationFolder"),
		"the simplified Language pane retains useful locale controls");
	expect(!preferencesSource.contains("UseCustomTheme")
		&& !preferencesSource.contains("kKeyTheme")
		&& !preferencesHeader.contains("useCustomTheme")
		&& !preferencesHeader.contains("ETheme"),
		"legacy stored theme keys are ignored rather than read, written, reset, or cached");
	expect(preferencesSource.contains("applySystemTheme();")
		&& themeSource.contains("applySystemTheme()")
		&& themeSource.contains("StyleNoCustom.qss")
		&& !themeSource.contains("setPalette")
		&& !themeSource.contains("setColorScheme")
		&& !themeSource.contains("lightPalette")
		&& !themeSource.contains("darkPalette")
		&& !themeSource.contains("StyleLight.qss")
		&& !themeSource.contains("StyleDark.qss"),
		"startup preserves the Windows and Qt system palette without forcing Light or Dark");
	expect(pickerDelegateSource.contains("option.palette.color")
		&& !pickerDelegateSource.contains("PreferencesManager")
		&& !pickerDelegateSource.contains("useCustomTheme"),
		"the picker follows its active system palette instead of a Lean theme preference");
	expect(!readSourceFile("Beeftext.qrc").contains("StyleCommon.qss")
		&& !readSourceFile("Beeftext.qrc").contains("StyleLight.qss")
		&& !readSourceFile("Beeftext.qrc").contains("StyleDark.qss")
		&& readSourceFile("Beeftext.qrc").contains("StyleNoCustom.qss"),
		"custom theme styles are removed while the palette-driven picker framing remains");
	expect(readRepositoryFile("README.md").contains("Lean Beeftext follows the Windows light or dark appearance setting.")
		&& readRepositoryFile("README.md").contains("restart Lean Beeftext to apply the change consistently."),
		"documentation states the system-appearance rule and restart caveat");

	QString const mainUi = readSourceFile("MainWindow.ui");
	QString const mainSource = readSourceFile("MainWindow.cpp");
	QString const entryPointSource = readSourceFile("main.cpp");
	QString const resourceSource = readSourceFile("Beeftext.qrc");
	QString const windowsResourceSource = readSourceFile("Beeftext.rc");
	expect(entryPointSource.contains("setApplicationName(constants::kSettingsApplicationName)")
		&& entryPointSource.contains("setApplicationDisplayName(constants::kApplicationName)")
		&& entryPointSource.contains("setApplicationVersion(constants::kProductVersion)"),
        "the public display name and permanent Lean settings identity are applied");
	expect(entryPointSource.contains("singleInstanceApp(constants::kSingleInstanceIdentifier)")
		&& !entryPointSource.contains("\"BeeftextSingleInstanceIdentifier\""),
		"single-instance enforcement uses the Lean identity instead of the upstream identifier");
	expect(mainSource.contains("removeAction(ui_.menu_Advanced->menuAction())")
		&& mainSource.contains("insertMenu(ui_.menu_Help->menuAction(), combosMenu_)")
		&& mainSource.contains("insertMenu(ui_.menu_Help->menuAction(), groupsMenu_)"),
		"Release menu construction yields File, Combos, Groups, Help without Advanced");
	expect(mainSource.contains("combosMenu_->addAction(ui_.actionGenerateCheatSheet)"),
		"Generate Cheat Sheet is reachable from the Combos menu");
	expect(windowsResourceSource.contains("Resources/Icons/LeanBeeftextApp.ico")
		&& resourceSource.contains("Resources/Icons/LeanBeeftextApp.ico")
		&& resourceSource.contains("Resources/Icons/LeanBeeftextAppPaused.ico")
		&& mainSource.contains("Resources/Icons/LeanBeeftextTray.ico")
		&& mainSource.contains("Resources/Icons/LeanBeeftextTrayPaused.ico")
		&& mainSource.contains("Resources/Icons/LeanBeeftextAppPaused.ico")
		&& mainSource.contains("setWindowIcon(windowIcon)"),
		"executable, window, enabled tray, and paused tray surfaces use their dedicated Lean icons");
	expect(readSourceFile("Dialogs/AboutDialog.ui").contains("Icons/App/LeanBeeftextApp-128.png")
		&& readSourceFile("Picker/PickerWindow.ui").contains("Icons/App/LeanBeeftextApp-32.png")
		&& !resourceSource.contains("BeeftextLogo128.png")
		&& !resourceSource.contains("BeeftextIconGrayscale.ico"),
		"About and picker use full Lean artwork while legacy compiled icon resources are absent");
	expect(readRepositoryFile("Scripts/GenerateLeanIconAssets.sh").contains("LeanBeeftextTray16Source.png")
		&& readSourceFile("Resources/Icons/ASSET_MANIFEST.md").contains("Dedicated optical bull-only artwork")
		&& readSourceFile("Resources/Icons/SHA256SUMS.txt").contains("LeanBeeftextTray-16.png"),
		"the deterministic asset bundle records and verifies the dedicated 16-pixel optical source");
	expect(mainUi.contains("Open &amp;Diagnostic Log")
		&& mainUi.contains("&amp;About Lean Beeftext"),
		"Help exposes Open Diagnostic Log and About Lean Beeftext");
	expect(mainUi.indexOf("actionVisitBeeftextWiki") < mainUi.indexOf("actionShowReleaseNotes")
		&& mainUi.indexOf("actionShowReleaseNotes") < mainUi.indexOf("actionReportBug")
		&& mainUi.indexOf("actionReportBug") < mainUi.indexOf("actionOpenLogFile"),
		"Help actions retain the requested order");

	QString const aboutUi = readSourceFile("Dialogs/AboutDialog.ui");
	QString const aboutSource = readSourceFile("Dialogs/AboutDialog.cpp");
	expect(aboutUi.contains("About Lean Beeftext")
		&& aboutUi.contains("Maintained by Jubal Slone")
		&& aboutUi.contains("unofficial fork of Beeftext 16.0 by Xavier Michelon")
		&& aboutUi.contains("upstream Beeftext translation contributors"),
		"About dialog has the correct maintainer, upstream author, and translator attribution");
	expect(aboutSource.contains("kProductVersion")
		&& aboutSource.contains("Project Repository")
		&& aboutSource.contains("Third-Party Notices"),
		"About dialog presents the public version and concise project/license links");

	QString const readme = readRepositoryFile("README.md");
	QString const securityModel = readRepositoryFile("SECURITY_MODEL.md");
	QString const notices = readRepositoryFile("THIRD_PARTY_NOTICES.md");
	QRegularExpression const variablesHeading(R"((?:^|\r?\n)## Variables(?:\r?\n|$))");
	expect(readme.contains(variablesHeading)
		&& readme.contains("Lean Beeftext 1.0.0")
		&& readme.contains("Des Moines, IA 50309"),
		"README contains the Variables anchor and Lean product guidance");
	expect(QFileInfo(QDir(QStringLiteral(BEEFTEXT_SOURCE_DIR)).absoluteFilePath("../SECURITY_MODEL.md")).isFile()
		&& !QFileInfo(QDir(QStringLiteral(BEEFTEXT_SOURCE_DIR)).absoluteFilePath("../TLF_SAFE_BUILD.md")).exists(),
		"SECURITY_MODEL.md replaces the old public security document filename");
	expect(!readme.contains("TLF", Qt::CaseInsensitive)
		&& !securityModel.contains("TLF", Qt::CaseInsensitive)
		&& !notices.contains("TLF", Qt::CaseInsensitive)
		&& !readme.contains("Trent Law Firm", Qt::CaseInsensitive)
		&& !securityModel.contains("Trent Law Firm", Qt::CaseInsensitive),
		"public documentation contains no old organization-specific terminology");
	expect(readSourceFile("Combo/ComboPortability.cpp").contains("Legacy Beeftext JSON files (*.json)")
		&& readSourceFile("Combo/ComboPortability.cpp").contains("Legacy Beeftext CSV files (*.csv)"),
		"legacy Beeftext import format labels remain correctly branded");

	QString const quickSearchSurfaces = readRepositoryFile("README.md")
		+ readSourceFile("Preferences/Panes/PrefPaneCombos.ui")
		+ readSourceFile("Preferences/Panes/PrefPaneEmojis.ui")
		+ readSourceFile("Preferences/Panes/PrefPaneCombos.cpp");
	expect(quickSearchSurfaces.contains("Quick Search")
		&& !quickSearchSurfaces.contains("Combo Picker", Qt::CaseInsensitive)
		&& !quickSearchSurfaces.contains("picker window", Qt::CaseInsensitive)
		&& entryPointSource.contains("The Quick Search shortcut")
		&& !entryPointSource.contains("\"The combo picker")
		&& readSourceFile("Picker/PickerWindow.h").contains("showComboPickerWindow")
		&& preferencesSource.contains("kKeyComboPickerShortcut"),
		"release UI and documentation use Quick Search while internal picker identifiers remain compatible");

	QString const currentProjectLinks = constantsSource + readRepositoryFile("README.md")
		+ readRepositoryFile("SECURITY_MODEL.md") + readRepositoryFile("INSTALLER.md")
		+ readRepositoryFile("Installer/LeanBeeftext.iss") + readRepositoryFile("Installer/StagePayload.ps1")
		+ aboutSource + readSourceFile("Combo/ComboDialog.cpp") + readSourceFile("Combo/ComboDialog.ui")
		+ readSourceFile("Combo/ComboImportDialog.ui") + appearanceUi
		+ readRepositoryFile(".github/CONTRIBUTING.md");
	expect(currentProjectLinks.contains("github.com/jubalslone/lean-beeftext")
		&& !currentProjectLinks.contains("github.com/jubalslone/Beeftext")
		&& currentProjectLinks.contains("github.com/xmichelo/Beeftext"),
		"current Lean links use the canonical repository while upstream attribution remains unchanged");
}


void testInstalledStorageAndMigrationSafety() {
    QString const globalsSource = readSourceFile("BeeftextGlobals.cpp");
    QString const preferencesSource = readSourceFile("Preferences/PreferencesManager.cpp");
    QString const autoStartSource = readSourceFile("AutoStart.cpp");
    QString const migrationSource = readSourceFile("Migration/LegacyMigrationManager.cpp");
    QString const comboLastUseSource = readSourceFile("LastUse/ComboLastUseFile.cpp");
    QString const emojiLastUseSource = readSourceFile("LastUse/EmojiLastUseFile.cpp");
    expect(globalsSource.contains("QStandardPaths::DocumentsLocation")
        && globalsSource.contains("Lean Beeftext")
        && globalsSource.contains("installedSettingsFilePath")
        && preferencesSource.contains("globals::installedSettingsFilePath(), QSettings::IniFormat"),
        "installed restorable data and explicit INI settings are rooted under the Documents known folder");
    expect(globalsSource.contains("SHGetKnownFolderPath(FOLDERID_LocalAppData")
        && globalsSource.contains("CoTaskMemFree(knownFolderPath)")
        && globalsSource.contains("QDir(localAppData).absoluteFilePath(\"Lean Beeftext\")")
        && !globalsSource.contains("QStandardPaths::AppLocalDataLocation")
        && !globalsSource.contains("Jubal Slone")
        && !globalsSource.contains("QDir(localAppData).absoluteFilePath(constants::kOrganizationName)")
        && globalsSource.contains("machineLocalDataDir")
        && globalsSource.contains("QDir(machineLocalDataDir()).absoluteFilePath(\"log.txt\")")
        && comboLastUseSource.contains("QDir(globals::machineLocalDataDir()).absoluteFilePath(kComboLastUseFileName)")
        && comboLastUseSource.contains("kComboLastUseFileName = \"comboLastUse.json\"")
        && emojiLastUseSource.contains("QDir(globals::machineLocalDataDir()).absoluteFilePath(kEmojiLastUseFileName)")
        && emojiLastUseSource.contains("kEmojiLastUseFileName = \"emojiLastUse.json\""),
        "installed logs and last-use caches use LocalAppData/Lean Beeftext without a publisher directory");
    expect(autoStartSource.contains("QCoreApplication::applicationFilePath()")
        && !autoStartSource.contains("kKeyAppExePath")
        && migrationSource.contains("AppExePath"),
        "autostart uses the current executable while legacy AppExePath is source evidence only");
    expect(readSourceFile("Combo/ComboList.cpp").contains("QSaveFile file(path)")
        && readSourceFile("Combo/ComboList.cpp").contains("file.commit()"),
        "live combo writes use atomic replacement");
    expect(preferencesSource.contains("globals::portableModeSettingsFilePath(), QSettings::IniFormat")
        && globalsSource.contains("appDir.absoluteFilePath(\"Data\")")
        && globalsSource.contains("if (isInPortableMode())")
        && globalsSource.contains("return portableModeDataFolderPath();")
        && globalsSource.contains("isInPortableMode() ? \"Backup\" : \"Backups\""),
        "portable Settings.ini, Data, and Data/Backup behavior remains unchanged");

    QTemporaryDir temporaryDirectory;
    expect(temporaryDirectory.isValid(), "migration safety test directory is available");
    if (!temporaryDirectory.isValid())
        return;
    QDir root(temporaryDirectory.path());
    QString const portable = root.absoluteFilePath("Beeftext Portable");
    QDir().mkpath(QDir(portable).absoluteFilePath("Data"));
    QFile executable(QDir(portable).absoluteFilePath("Beeftext.exe"));
    expect(executable.open(QIODevice::WriteOnly) && executable.write("fixture") > 0,
        "portable fixture executable is created");
    executable.close();
    QFile beacon(QDir(portable).absoluteFilePath("Portable.bin"));
    expect(beacon.open(QIODevice::WriteOnly), "portable fixture beacon is created");
    beacon.close();
    QFile combos(QDir(portable).absoluteFilePath("Data/comboList.json"));
    expect(combos.open(QIODevice::WriteOnly) && combos.write("{\"combos\":[]}") > 0,
        "portable fixture combo library is created");
    combos.close();
    QString comboPath;
    QString cleanupRoot;
    migration::EPortableProduct upstreamProduct = migration::EPortableProduct::LeanBeeftext;
    expect(migration::isStrongPortableCandidate(portable, &comboPath, &cleanupRoot, &upstreamProduct)
        && upstreamProduct == migration::EPortableProduct::UpstreamBeeftext
        && QFileInfo(comboPath).fileName() == "comboList.json"
        && QFileInfo(cleanupRoot).fileName() == "Beeftext Portable",
        "strong portable detection requires the expected beacon/data layout");
    QFile::remove(QDir(portable).absoluteFilePath("Portable.bin"));
    expect(!migration::isStrongPortableCandidate(portable),
        "an arbitrary Beeftext.exe plus data is rejected without a portable beacon");

    QString const leanPortable = root.absoluteFilePath("release-47");
    QDir().mkpath(QDir(leanPortable).absoluteFilePath("Data"));
    QFile leanExecutable(QDir(leanPortable).absoluteFilePath("LeanBeeftext.exe"));
    expect(leanExecutable.open(QIODevice::WriteOnly) && leanExecutable.write("lean fixture") > 0,
        "Lean portable fixture executable is created");
    leanExecutable.close();
    QFile leanBeacon(QDir(leanPortable).absoluteFilePath("Portable.bin"));
    expect(leanBeacon.open(QIODevice::WriteOnly), "Lean portable fixture beacon is created");
    leanBeacon.close();
    QFile leanCombos(QDir(leanPortable).absoluteFilePath("Data/comboList.json"));
    expect(leanCombos.open(QIODevice::WriteOnly) &&
        leanCombos.write("{\"fileFormatVersion\":3,\"groups\":[],\"combos\":[]}") > 0,
        "Lean portable fixture combo library is created");
    leanCombos.close();
    migration::EPortableProduct portableProduct = migration::EPortableProduct::UpstreamBeeftext;
    expect(migration::isStrongPortableCandidate(leanPortable, &comboPath, &cleanupRoot, &portableProduct)
        && portableProduct == migration::EPortableProduct::LeanBeeftext
        && QFileInfo(cleanupRoot).fileName() == "release-47",
        "LeanBeeftext.exe plus Portable.bin and readable combo data identifies Lean portable regardless of folder name");
    expect(leanCombos.open(QIODevice::WriteOnly | QIODevice::Truncate) && leanCombos.write("not JSON") > 0,
        "malformed Lean portable combo fixture is written");
    leanCombos.close();
    expect(!migration::isStrongPortableCandidate(leanPortable),
        "LeanBeeftext.exe plus Portable.bin is rejected when combo data is malformed");
    QFile::remove(QDir(leanPortable).absoluteFilePath("Data/comboList.json"));
    expect(!migration::isStrongPortableCandidate(leanPortable),
        "LeanBeeftext.exe plus Portable.bin is rejected without readable combo data");

    QString const loneLean = root.absoluteFilePath("lone-application");
    QDir().mkpath(loneLean);
    QFile loneLeanExecutable(QDir(loneLean).absoluteFilePath("LeanBeeftext.exe"));
    expect(loneLeanExecutable.open(QIODevice::WriteOnly) && loneLeanExecutable.write("lean fixture") > 0,
        "lone Lean executable fixture is created");
    loneLeanExecutable.close();
    expect(!migration::isStrongPortableCandidate(loneLean),
        "a lone LeanBeeftext.exe is not classified as portable");

    QString const portableAppsRoot = root.absoluteFilePath("portable-apps-fixture");
    QString const portableAppsExecutableFolder = QDir(portableAppsRoot).absoluteFilePath("App/LeanBeeftext");
    QDir().mkpath(portableAppsExecutableFolder);
    QDir().mkpath(QDir(portableAppsRoot).absoluteFilePath("Data/settings"));
    QFile portableAppsExecutable(QDir(portableAppsExecutableFolder).absoluteFilePath("LeanBeeftext.exe"));
    expect(portableAppsExecutable.open(QIODevice::WriteOnly) && portableAppsExecutable.write("lean fixture") > 0,
        "Lean PortableApps executable fixture is created");
    portableAppsExecutable.close();
    QFile portableAppsBeacon(QDir(portableAppsExecutableFolder).absoluteFilePath("PortableApps.bin"));
    expect(portableAppsBeacon.open(QIODevice::WriteOnly), "Lean PortableApps beacon is created");
    portableAppsBeacon.close();
    QFile portableAppsCombos(QDir(portableAppsRoot).absoluteFilePath("Data/settings/comboList.json"));
    expect(portableAppsCombos.open(QIODevice::WriteOnly) &&
        portableAppsCombos.write("{\"fileFormatVersion\":3,\"groups\":[],\"combos\":[]}") > 0,
        "Lean PortableApps combo library is created");
    portableAppsCombos.close();
    portableProduct = migration::EPortableProduct::UpstreamBeeftext;
    expect(migration::isStrongPortableCandidate(portableAppsExecutableFolder, &comboPath, &cleanupRoot, &portableProduct)
        && portableProduct == migration::EPortableProduct::LeanBeeftext
        && QDir::cleanPath(cleanupRoot) == QDir::cleanPath(portableAppsRoot),
        "the existing PortableApps beacon and Data/settings layout recognizes portable Lean Beeftext");

    expect(migration::isBroadCleanupRoot(QString(), { root.absolutePath() })
		&& migration::isBroadCleanupRoot(root.absolutePath(), { root.absolutePath() })
        && !migration::isBroadCleanupRoot(portable, { root.absolutePath() }),
        "broad protected roots are refused while a dedicated child folder is eligible");
	expect(migration::isRecognizableInstalledCandidate("Beeftext", "Xavier Michelon", portable,
		QDir(portable).absoluteFilePath("Beeftext.exe"))
		&& !migration::isRecognizableInstalledCandidate("Lean Beeftext", "Jubal Slone", portable,
			QDir(portable).absoluteFilePath("Beeftext.exe"))
		&& !migration::isRecognizableInstalledCandidate("Beeftext", "Xavier Michelon", leanPortable,
			QDir(portable).absoluteFilePath("Beeftext.exe")),
		"installed detection accepts exact upstream identity and rejects Lean or an executable outside the recorded root");

	QByteArray const preCloseContents("digest A");
	QByteArray const postCloseContents("digest B");
	QByteArray const changedAgainContents("digest C");
	QByteArray const preCloseDigest = migration::sourceContentDigest(preCloseContents);
	QByteArray const refreshedDigest = migration::sourceContentDigest(postCloseContents);
	expect(preCloseDigest != refreshedDigest
		&& migration::sourceContentMatchesDigest(postCloseContents, refreshedDigest)
		&& !migration::sourceContentMatchesDigest(postCloseContents, preCloseDigest)
		&& !migration::sourceContentMatchesDigest(changedAgainContents, refreshedDigest),
		"a post-close digest refresh accepts the exact imported bytes and refuses a later source change");

    QList<QList<qsizetype>> const groups = migration::groupSourcesByContent({ "same", "different", "same" });
    expect(groups.size() == 2 && groups[0] == QList<qsizetype>({ 0, 2 }) && groups[1] == QList<qsizetype>({ 1 }),
        "identical libraries group together while differing libraries remain separate choices");
    migration::ValidationResult validation;
    validation.snapshotCreated = validation.parsed = validation.persisted = validation.reloaded = true;
    expect(!migration::cleanupAllowed(validation), "cleanup is refused until correspondence validation succeeds");
    validation.corresponds = true;
    expect(migration::cleanupAllowed(validation), "cleanup is allowed only after the full validation sequence");
    expect(migration::shouldRunMigration(false, false, migration::EState::NeverChecked)
        && !migration::shouldRunMigration(true, false, migration::EState::NeverChecked)
        && !migration::shouldRunMigration(false, true, migration::EState::NeverChecked)
        && !migration::shouldRunMigration(false, false, migration::EState::ImportCompleted),
        "migration is installed-only, first-run, non-overwriting, and idempotent");
    expect(migrationSource.contains("migrateSource(selected, validation, error)")
        && migrationSource.contains("cleanupAllowed(validation)")
        && migrationSource.contains("ImportCompleted")
        && migrationSource.indexOf("settings.setValue(kMigrationStateKey, int(migration::EState::ImportCompleted))")
            < migrationSource.indexOf("finishPendingCleanup(settings, false)"),
		"the runtime validates installed migration and persists successful import state before portable cleanup so retries cannot re-import");
	expect(migrationSource.contains("refreshSourceBeforeImport(sources[index], refreshed, refreshError)")
		&& migrationSource.indexOf("refreshSourceBeforeImport(sources[index], refreshed, refreshError)")
			< migrationSource.indexOf("migrateSource(selected, validation, error)")
		&& migrationSource.contains("readSourceContentsMatchingDigest(source, sourceContents, outError)")
		&& migrationSource.contains("writeRecoverySnapshot(source, sourceContents, snapshotFolder, outError)")
		&& migrationSource.contains("QJsonDocument::fromJson(sourceContents")
		&& migrationSource.contains("source.comboDigest != selected.comboDigest"),
		"selected sources refresh after graceful close and one digest-bound byte sequence drives snapshot, parse, import, and cleanup");
	expect(!migrationSource.contains("UninstallString")
		&& !migrationSource.contains("QuietUninstallString")
		&& !migrationSource.contains("ShellExecuteExW")
		&& !migrationSource.contains("_?=")
		&& !migrationSource.contains("Remove the old Beeftext installation")
		&& !migrationSource.contains("cmd.exe")
		&& !migrationSource.contains("powershell", Qt::CaseInsensitive),
		"installed upstream migration exposes and executes no automatic uninstaller path or installed cleanup checkbox");
	expect(migrationSource.contains("uninstallRegistryHive")
		&& migrationSource.contains("uninstallRegistrySubkey")
		&& migrationSource.contains("uninstallRegistryView")
		&& migrationSource.contains("refreshRegisteredInstalledSource")
		&& migrationSource.contains("retired pre-release pending installed cleanup without invoking an upstream uninstaller")
		&& migrationSource.contains("portablePending.append(value)")
		&& !migrationSource.contains("object[\"uninstallRegistryHive\"] =")
		&& !migrationSource.contains("object[\"uninstallRegistrySubkey\"] =")
		&& !migrationSource.contains("object[\"uninstallRegistryView\"] ="),
		"registry identity still refreshes the exact installed source while obsolete installed pending cleanup is retired and no longer serialized");
	expect(!migrationSource.contains("addInfo(sourceContents")
		&& !migrationSource.contains("addWarning(sourceContents")
		&& !migrationSource.contains(".arg(sourceContents)")
		&& !migrationSource.contains(".arg(contents)"),
		"local diagnostics do not log combo contents");
	expect(migrationSource.contains("Beeftext is currently running")
		&& migrationSource.contains("Close Beeftext and continue")
		&& migrationSource.contains("requestGracefulClose(source)")
		&& migrationSource.contains("runningProcessesForExecutable(source.executablePath)")
		&& migrationSource.contains("RmRegisterResources")
		&& migrationSource.contains("RmShutdown(session, 0, nullptr)")
		&& !migrationSource.contains("TerminateProcess")
		&& !migrationSource.contains("RmForceShutdown")
		&& migrationSource.contains("ERunningSourceDecision::Cancelled")
		&& migrationSource.indexOf("closeRunningSourceWithConsent(sources[index])")
			< migrationSource.indexOf("migrateSource(selected, validation, error)"),
		"the exact running source is offered a consented graceful close before import without forced termination");
	expect(migrationSource.contains("Import from Beeftext → Lean Beeftext"),
		"the migration title shows the source-to-destination arrow");
	qsizetype const recommendationStart = migrationSource.indexOf("void showInstalledBeeftextRecommendation()");
	qsizetype const recommendationEnd = migrationSource.indexOf("bool finishPendingCleanup(", recommendationStart);
	QString const recommendationImplementation = migrationSource.mid(recommendationStart,
		recommendationEnd - recommendationStart);
	expect(recommendationStart >= 0 && recommendationEnd > recommendationStart
		&& recommendationImplementation.contains("Beeftext is still installed")
		&& recommendationImplementation.contains("We strongly recommend uninstalling Beeftext.")
		&& recommendationImplementation.contains("Running Beeftext and Lean Beeftext at the same time")
		&& !recommendationImplementation.contains("before continuing", Qt::CaseInsensitive)
		&& recommendationImplementation.contains("Open Installed Apps")
		&& recommendationImplementation.contains("prompt.setDefaultButton(openButton)")
		&& recommendationImplementation.contains("QDesktopServices::openUrl(QUrl(\"ms-settings:appsfeatures\"))")
		&& recommendationImplementation.contains("verified migration remains complete")
		&& migrationSource.contains("if (importedFromInstalledBeeftext)")
		&& migrationSource.lastIndexOf("showInstalledBeeftextRecommendation();")
			> migrationSource.indexOf("migrateSource(selected, validation, error)"),
		"successful installed migration recommends manual uninstall and opens Installed Apps directly without making Settings part of migration success");
	qsizetype const cleanupStart = migrationSource.indexOf("bool cleanupSource(");
	qsizetype const cleanupEnd = migrationSource.indexOf("bool installedSourceStillPresent(", cleanupStart);
	QString const cleanupImplementation = migrationSource.mid(cleanupStart, cleanupEnd - cleanupStart);
	expect(cleanupStart >= 0 && cleanupEnd > cleanupStart
		&& cleanupImplementation.contains("source.type != migration::ESourceType::Portable")
		&& !cleanupImplementation.contains("beeftext.org/Beeftext")
		&& !cleanupImplementation.contains("legacyDefaultComboFilePath")
		&& !cleanupImplementation.contains("removeRecursively")
		&& !cleanupImplementation.contains("Uninstall"),
		"cleanup is portable-only and never deletes legacy upstream AppData or invokes installed removal");
    qsizetype const shallowStart = migrationSource.indexOf("QStringList const shallowRoots");
    qsizetype const shallowEnd = migrationSource.indexOf("for (qsizetype i = sources.size()", shallowStart);
    QString const shallowDiscovery = migrationSource.mid(shallowStart, shallowEnd - shallowStart);
    expect(shallowStart >= 0 && shallowEnd > shallowStart
        && shallowDiscovery.contains("QStandardPaths::DesktopLocation")
        && shallowDiscovery.contains("QStandardPaths::DownloadLocation")
        && shallowDiscovery.contains("directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)")
        && !shallowDiscovery.contains("QDirIterator")
        && !shallowDiscovery.contains("Subdirectories"),
        "Desktop and Downloads discovery checks each root and its immediate children without recursive searching");
    expect(migrationSource.contains("Portable Beeftext")
        && migrationSource.contains("Portable Lean Beeftext")
        && migrationSource.contains("Remove the portable Lean Beeftext copy after import succeeds (Recommended)")
        && migrationSource.contains("Remove the detected portable copies after import succeeds (Recommended)"),
        "migration UI distinguishes upstream and Lean portable sources and uses accurate cleanup wording");
    expect(migrationSource.contains("product != source.portableProduct")
		&& migrationSource.contains("currentDigest != source.comboDigest")
        && migrationSource.contains("QFileInfo(rootPath).fileName().contains(\"beeftext\", Qt::CaseInsensitive)")
        && migrationSource.contains("FOF_ALLOWUNDO")
        && migrationSource.contains("return recyclePaths(recycle)")
        && migrationSource.contains("samePath(shortcutTarget(shortcut), source.executablePath)")
        && migrationSource.contains("object[\"portableProduct\"]")
        && !migrationSource.contains("portableModeSettingsFilePath")
        && !migrationSource.contains("Settings.ini"),
        "portable cleanup revalidates product/content and uses the Recycle Bin while portable settings are not imported");
    expect(migrationSource.contains("QSettings legacySettings(\"beeftext.org\", \"Beeftext\")")
        && !preferencesSource.contains("QSettings>(constants::kOrganizationName, constants::kSettingsApplicationName)"),
        "upstream preferences are read only as migration clues and are not adopted as Lean preferences");
}


void testInstallerArchitecture() {
    QString const installer = readRepositoryFile("Installer/LeanBeeftext.iss");
    QString const staging = readRepositoryFile("Installer/StagePayload.ps1");
    QString const workflow = readRepositoryFile(".github/workflows/windows-build.yml");
    QString const installerDoc = readRepositoryFile("INSTALLER.md");
    expect(installer.contains("AppId={{499E5EE9-ECC6-455E-B78A-EDF581715A80}")
        && !installer.contains("AppId={{499E5EE9-ECC6-455E-B78A-EDF581715A80}-1.0")
        && installerDoc.contains("{499E5EE9-ECC6-455E-B78A-EDF581715A80}"),
        "the Inno AppId is explicit, stable, documented, and version-free");
    expect(installer.contains("DefaultDirName={autopf}\\Lean Beeftext")
        && installer.contains("PrivilegesRequired=admin")
        && installer.contains("Flags: unchecked")
        && installer.contains("skipifsilent runasoriginaluser")
        && !installer.contains("LicenseFile=")
        && !installer.contains("Portable.bin"),
        "the single installer uses the boring elevated Program Files flow without portable state or a license page");
	expect(installer.contains("CloseApplications=yes")
		&& !installer.contains("CloseApplications=force")
        && installer.contains("RestartApplications=no")
        && installer.contains("CompareVersions")
        && installer.contains("newer version of Lean Beeftext")
        && installerDoc.contains("/VERYSILENT /SUPPRESSMSGBOXES /NORESTART"),
		"installer upgrades permit same-version reinstall, refuse downgrade, avoid force-closing, and document unattended use");
	QString const singleInstanceSource = readRepositoryFile("Submodules/XMiLib/XMiLib/SingleInstanceApp.cpp");
	expect(singleInstanceSource.contains("QSharedMemory") ||
		readRepositoryFile("Submodules/XMiLib/XMiLib/SingleInstanceApp.h").contains("QSharedMemory"),
		"the XMiLib single-instance implementation is verified as Qt shared memory rather than an assumed Windows mutex");
	expect(!installer.contains("AppMutex=")
		&& installer.contains("WbemScripting.SWbemLocator")
		&& installer.contains("SELECT ExecutablePath FROM Win32_Process")
		&& installer.contains("SameText(ProcessPath, ExpectedPath)")
		&& installer.contains("function InitializeUninstall(): Boolean")
		&& installer.contains("UninstallSilent()")
		&& installer.contains("SuppressibleMsgBox(")
		&& installer.contains("MB_RETRYCANCEL, IDCANCEL")
		&& installer.contains("Please close Lean Beeftext before uninstalling it.")
		&& !installer.contains("TerminateProcess")
		&& !installer.contains("taskkill", Qt::CaseInsensitive),
		"Lean uninstall checks the exact installed executable path, offers Retry/Cancel, aborts silently, and never force-terminates");
	expect(installer.contains("[Messages]")
		&& installer.contains("ConfirmUninstall=Are you sure you want to remove Lean Beeftext and its installed components?%n%nYour Lean Beeftext user data will not be removed.")
		&& installer.contains("UninstalledAll=Lean Beeftext was successfully removed.%n%nYour user data was kept."),
		"supported Inno messages explain that uninstall preserves Lean user data");
	expect(staging.contains("ValidateSet('Installed', 'Portable')")
			&& staging.contains("[string]$Repository = 'jubalslone/lean-beeftext'")
	        && staging.contains("Installed payload must not contain $beacon")
			&& staging.contains("MSVC runtime DLLs already staged by windeployqt")
			&& staging.contains("MSVC runtime DLLs added by explicit copy")
			&& staging.contains("$checksumFullPath = [IO.Path]::GetFullPath($checksumPath)")
        && staging.contains("SHA256SUMS.txt")
        && staging.contains("BUILD_INFO.txt"),
		"one staging script builds isolated installed and portable payloads with provenance manifests");
	expect(workflow.contains("Forbidden development/debug payload file")
		&& workflow.contains("Unexpected executable")
		&& workflow.contains("Qt debug DLL")
		&& workflow.contains("MSVCP140.dll")
		&& workflow.contains("VCRUNTIME140.dll")
		&& workflow.contains("VCRUNTIME140_1.dll")
		&& workflow.contains("Silent uninstall proceeded while the exact installed Lean process path was running")
		&& workflow.contains("Bounded running-process fixture did not exit normally"),
		"Windows CI rejects debug/development payloads, verifies runtimes, and exercises fail-closed silent uninstall");
    expect(!QFileInfo(QDir(QStringLiteral(BEEFTEXT_SOURCE_DIR)).absoluteFilePath("../Installer/installer.nsi")).exists()
        && !QFileInfo(QDir(QStringLiteral(BEEFTEXT_SOURCE_DIR)).absoluteFilePath("../Installer/BuildAll.ps1")).exists(),
        "obsolete NSIS entry points are retired");
    expect(workflow.contains("innosetup-7.1.0-x64.exe")
        && workflow.contains("0362a383ed217d4c4239b5933866dd96d3eb2102737da92f80f6057a4b40df2f")
		&& workflow.contains("Repository: jubalslone/lean-beeftext")
        && workflow.contains("-Mode Installed")
        && workflow.contains("-Mode Portable")
		&& workflow.contains("$checksumFullPath = [IO.Path]::GetFullPath($checksumPath)")
        && workflow.contains("Same-version reinstall")
        && workflow.contains("Documents user-data fixture was deleted by uninstall")
        && workflow.contains("Lean-Beeftext-Setup-1.0.0.exe"),
        "Windows CI pins and verifies Inno, stages both modes, and smoke-tests reinstall and preserving user data");
    expect(installerDoc.contains("does not delete `<Documents>\\Lean Beeftext`")
        && readRepositoryFile("README.md").contains("Program Files\\Lean Beeftext")
        && readRepositoryFile("README.md").contains("OneDrive Known Folder Move"),
        "installed locations, OneDrive redirection, and uninstall preservation are documented");
}

void testProductionSigningArchitecture() {
	QString const production = readRepositoryFile(".github/workflows/artifact-signing-release.yml");
	QString const smoke = readRepositoryFile(".github/workflows/azure-signing-smoke-test.yml");
	QString const installer = readRepositoryFile("Installer/LeanBeeftext.iss");
	QString const wrapper = readRepositoryFile("Installer/Invoke-ArtifactSigning.ps1");
	QString const signingDoc = readRepositoryFile("ARTIFACT_SIGNING.md");
	QString const routineWorkflow = readRepositoryFile(".github/workflows/windows-build.yml");

	expect(production.contains("workflow_dispatch:")
		&& !production.contains("pull_request:")
		&& !production.contains("\n  push:")
		&& !production.contains("\n  release:")
		&& production.contains("source_commit:")
		&& production.contains("^[0-9a-f]{40}$")
		&& production.contains("ref: ${{ inputs.source_commit }}")
		&& production.contains("persist-credentials: false"),
		"production signing is manual-only and checks out one explicit reviewed commit without persisted credentials");
	expect(production.contains("contents: read")
		&& production.contains("id-token: write")
		&& production.contains("environment: production-signing")
		&& production.contains("azure/login@a641126d1b8aa4d1fa005f4f92df94a3a4c4c906")
		&& production.contains("Azure/artifact-signing-action@c7ab2a863ab5f9a846ddb8265964877ef296ee82")
		&& production.contains("exclude-environment-credential: true")
		&& !production.contains("AZURE_CLIENT_SECRET"),
		"production signing uses the protected environment and pinned GitHub OIDC actions without a client secret");
	expect(smoke.contains("workflow_dispatch:")
		&& smoke.contains("Azure/artifact-signing-action@c7ab2a863ab5f9a846ddb8265964877ef296ee82"),
		"the successful manual smoke-test reference remains in the repository");

	expect(installer.contains("#ifdef ProductionSigning")
		&& installer.contains("SignTool=leanartifact")
		&& installer.contains("SignedUninstaller=yes")
		&& production.contains("--define=ProductionSigning")
		&& production.contains("--signtool=leanartifact=$signToolCommand")
		&& !routineWorkflow.contains("--define=ProductionSigning"),
		"Inno's documented signing integration is enabled only for the production installer and generated uninstaller");
	expect(wrapper.contains("[string]$FilePath")
		&& wrapper.contains("IndexOfAny([char[]]'*?')")
		&& wrapper.contains("Resolve-Path -LiteralPath")
		&& wrapper.contains("$target.Extension -ieq '.exe'")
		&& wrapper.contains("ArtifactSigning PowerShell module 0.1.8")
		&& wrapper.contains("Invoke-ArtifactSigning @signingParameters")
		&& wrapper.contains("ExcludeAzurePowerShellCredential = $true")
		&& !wrapper.contains("ExcludeAzureCliCredential = $true"),
		"the Inno bridge signs one explicit executable through the pinned module and OIDC-backed Azure CLI credential");
	expect(wrapper.contains("FileDigest = 'SHA256'")
		&& wrapper.contains("TimestampRfc3161 = 'http://timestamp.acs.microsoft.com'")
		&& wrapper.contains("TimestampDigest = 'SHA256'")
		&& wrapper.contains("$signature.Status -ne 'Valid'")
		&& wrapper.contains("$signature.SignerCertificate")
		&& wrapper.contains("$signature.TimeStamperCertificate"),
		"the signing bridge requires SHA-256 Authenticode plus a valid RFC 3161 timestamp");

	qsizetype const appSign = production.indexOf("Sign the one LeanBeeftext executable");
	qsizetype const portableFinalize = production.indexOf("reuse it for portable");
	qsizetype const innoBuild = production.indexOf("Compile and Authenticode-sign installer");
	qsizetype const installedVerification = production.indexOf("Assert-InstalledSignatures");
	qsizetype const finalHashes = production.indexOf("Generate final signed-candidate hashes");
	expect(appSign >= 0 && portableFinalize > appSign && innoBuild > portableFinalize
		&& installedVerification > innoBuild && finalHashes > installedVerification,
		"production ordering signs the app before portable packaging, then signs Inno outputs, verifies the deployed uninstaller, and hashes final bytes");
	expect(production.contains("Portable packaging does not contain the exact signed installed executable bytes.")
		&& production.contains("Installed LeanBeeftext.exe differs from the signed staged executable.")
		&& production.contains("has no RFC 3161 timestamp certificate")
		&& production.contains("signer subject does not match the signed app identity")
		&& production.contains("DISTRIBUTION_SHA256SUMS.txt")
		&& production.contains("PRODUCTION_SIGNING_REPORT.txt")
		&& production.contains("Lean-Beeftext-1.0.0-signed-portable-windows-x64")
		&& production.contains("Lean-Beeftext-1.0.0-signed-installer"),
		"the signed candidate fails closed on missing identity/timestamps and publishes only private QA artifacts with final hashes and provenance");
	expect(signingDoc.contains("generated uninstaller")
		&& signingDoc.contains("final `Lean-Beeftext-Setup-1.0.0.exe` installer")
		&& signingDoc.contains("GitHub's short-lived OIDC identity")
		&& signingDoc.contains("does not re-sign Qt")
		&& signingDoc.contains("does not guarantee that Microsoft Defender SmartScreen will never warn"),
		"signing documentation states the three-file scope, short-lived authentication, third-party boundary, and SmartScreen limitation");
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
	testProductFinishingSurface();
    testInstalledStorageAndMigrationSafety();
    testInstallerArchitecture();
	testProductionSigningArchitecture();
    if (failureCount == 0)
        qInfo() << "All Lean Beeftext security-model tests passed.";
    return failureCount == 0 ? 0 : 1;
}
