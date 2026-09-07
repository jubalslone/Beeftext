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
        && constantsHeader.contains("kSettingsApplicationName"),
        "installed settings and AppLocalData use permanent Lean identities");
	expect(constantsSource.contains("https://github.com/jubalslone/Beeftext#variables"),
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
}


void testInstalledStorageAndMigrationSafety() {
    QString const globalsSource = readSourceFile("BeeftextGlobals.cpp");
    QString const preferencesSource = readSourceFile("Preferences/PreferencesManager.cpp");
    QString const autoStartSource = readSourceFile("AutoStart.cpp");
    QString const migrationSource = readSourceFile("Migration/LegacyMigrationManager.cpp");
    expect(globalsSource.contains("QStandardPaths::DocumentsLocation")
        && globalsSource.contains("Lean Beeftext")
        && globalsSource.contains("installedSettingsFilePath")
        && preferencesSource.contains("globals::installedSettingsFilePath(), QSettings::IniFormat"),
        "installed restorable data and explicit INI settings are rooted under the Documents known folder");
    expect(globalsSource.contains("QStandardPaths::AppLocalDataLocation")
        && globalsSource.contains("machineLocalDataDir")
        && readSourceFile("LastUse/ComboLastUseFile.cpp").contains("machineLocalDataDir")
        && readSourceFile("LastUse/EmojiLastUseFile.cpp").contains("machineLocalDataDir"),
        "logs and last-use caches use Lean machine-local storage");
    expect(autoStartSource.contains("QCoreApplication::applicationFilePath()")
        && !autoStartSource.contains("kKeyAppExePath")
        && migrationSource.contains("AppExePath"),
        "autostart uses the current executable while legacy AppExePath is source evidence only");
    expect(readSourceFile("Combo/ComboList.cpp").contains("QSaveFile file(path)")
        && readSourceFile("Combo/ComboList.cpp").contains("file.commit()"),
        "live combo writes use atomic replacement");
    expect(preferencesSource.contains("globals::portableModeSettingsFilePath(), QSettings::IniFormat")
        && globalsSource.contains("appDir.absoluteFilePath(\"Data\")")
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
    expect(migration::isStrongPortableCandidate(portable, &comboPath, &cleanupRoot)
        && QFileInfo(comboPath).fileName() == "comboList.json"
        && QFileInfo(cleanupRoot).fileName() == "Beeftext Portable",
        "strong portable detection requires the expected beacon/data layout");
    QFile::remove(QDir(portable).absoluteFilePath("Portable.bin"));
    expect(!migration::isStrongPortableCandidate(portable),
        "an arbitrary Beeftext.exe plus data is rejected without a portable beacon");

    expect(migration::isBroadCleanupRoot(root.absolutePath(), { root.absolutePath() })
        && !migration::isBroadCleanupRoot(portable, { root.absolutePath() }),
        "broad protected roots are refused while a dedicated child folder is eligible");
    expect(migration::installedMetadataIsConsistent("Beeftext", "Xavier Michelon", portable,
        QDir(portable).absoluteFilePath("uninstall.exe"), QDir(portable).absoluteFilePath("Beeftext.exe"))
        && !migration::installedMetadataIsConsistent("Lean Beeftext", "Jubal Slone", portable,
            QDir(portable).absoluteFilePath("uninstall.exe"), QDir(portable).absoluteFilePath("Beeftext.exe")),
        "installed cleanup metadata accepts upstream identity and rejects Lean identity");

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
        "the runtime persists successful import state before cleanup so retries cannot re-import");
    expect(migrationSource.contains("QSettings legacySettings(\"beeftext.org\", \"Beeftext\")")
        && !preferencesSource.contains("QSettings>(constants::kOrganizationName, constants::kSettingsApplicationName)"),
        "upstream preferences are read only as migration clues and are not adopted as Lean preferences");
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
    if (failureCount == 0)
        qInfo() << "All Lean Beeftext security-model tests passed.";
    return failureCount == 0 ? 0 : 1;
}
