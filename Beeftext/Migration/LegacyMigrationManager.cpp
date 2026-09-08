/// \file
/// \brief One-time installed-mode migration from upstream Beeftext.


#include "stdafx.h"
#include "LegacyMigrationManager.h"
#include "LegacyMigrationCore.h"
#include "BeeftextGlobals.h"
#include "BeeftextUtils.h"
#include "Combo/ComboList.h"
#include "Preferences/PreferencesManager.h"

#ifdef _WIN32
#include <TlHelp32.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#endif
#include <vector>


namespace {


QString const kMigrationStateKey = "Migration/UpstreamBeeftextState";
QString const kPendingCleanupKey = "Migration/PendingCleanup";


struct LegacySource {
    migration::ESourceType type { migration::ESourceType::Portable };
    QString rootPath;
    QString executablePath;
    QString comboFilePath;
    QString displayName;
    QString publisher;
    QString uninstallCommand;
    QString quietUninstallCommand;
    QStringList shortcutPaths;
    QByteArray comboDigest;
    QDateTime modified;
    bool cleanupSafe { false };
};


QByteArray fileDigest(QString const &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256) : QByteArray();
}


QString canonicalPath(QString const &path) {
    QFileInfo const info(path);
    QString result = info.canonicalFilePath();
    return QDir::cleanPath(result.isEmpty() ? info.absoluteFilePath() : result);
}


bool samePath(QString const &left, QString const &right) {
    return canonicalPath(left).compare(canonicalPath(right), Qt::CaseInsensitive) == 0;
}


QString legacyDefaultComboFilePath() {
    QString const localAppData = qEnvironmentVariable("LOCALAPPDATA");
    return localAppData.isEmpty() ? QString() :
        QDir(localAppData).absoluteFilePath("beeftext.org/Beeftext/comboList.json");
}


QString legacyConfiguredComboFilePath() {
    QSettings legacySettings("beeftext.org", "Beeftext");
    QString folder = legacySettings.value("ComboListFolderPath").toString();
    if (folder.isEmpty())
        return legacyDefaultComboFilePath();
    QString const configured = QDir(QDir::fromNativeSeparators(folder)).absoluteFilePath(ComboList::defaultFileName);
    return QFileInfo(configured).isFile() ? configured : legacyDefaultComboFilePath();
}


QString legacyConfiguredExecutablePath() {
    QSettings legacySettings("beeftext.org", "Beeftext");
    return QDir::fromNativeSeparators(legacySettings.value("AppExePath").toString());
}


#ifdef _WIN32


QString registryString(HKEY key, wchar_t const *valueName) {
    DWORD type = 0;
    DWORD bytes = 0;
    if (RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) || bytes == 0)
        return QString();
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
    if (RegQueryValueExW(key, valueName, nullptr, &type, reinterpret_cast<BYTE *>(buffer.data()), &bytes) != ERROR_SUCCESS)
        return QString();
    QString result = QString::fromWCharArray(buffer.data()).trimmed();
    if (type == REG_EXPAND_SZ) {
        std::vector<wchar_t> expanded(32768, L'\0');
        DWORD const count = ExpandEnvironmentStringsW(reinterpret_cast<LPCWSTR>(result.utf16()), expanded.data(), DWORD(expanded.size()));
        if (count > 0 && count < expanded.size())
            result = QString::fromWCharArray(expanded.data());
    }
    return QDir::fromNativeSeparators(result);
}


QString executableFromCommand(QString const &command) {
    QStringList const parts = QProcess::splitCommand(command.trimmed());
    return parts.isEmpty() ? QString() : QDir::fromNativeSeparators(parts.first());
}


QString parametersFromCommand(QString const &command) {
    QString const trimmed = command.trimmed();
    if (trimmed.startsWith('"')) {
        qsizetype const closingQuote = trimmed.indexOf('"', 1);
        return closingQuote < 0 ? QString() : trimmed.mid(closingQuote + 1).trimmed();
    }
    qsizetype const whitespace = trimmed.indexOf(QRegularExpression("\\s"));
    return whitespace < 0 ? QString() : trimmed.mid(whitespace + 1).trimmed();
}


QString safeRegisteredUninstallCommand(LegacySource const &source) {
	QStringList const commands = source.quietUninstallCommand.isEmpty()
		? QStringList { source.uninstallCommand }
		: QStringList { source.quietUninstallCommand, source.uninstallCommand };
	for (QString const &command: commands) {
		QString const executable = executableFromCommand(command);
		if (migration::installedMetadataIsConsistent(source.displayName, source.publisher,
			source.rootPath, command, source.executablePath) && QFileInfo(executable).isFile())
			return command;
	}
	return QString();
}


QList<LegacySource> registeredInstalledSources() {
    QList<LegacySource> result;
    QString const configuredCombo = legacyConfiguredComboFilePath();
    QString const configuredExe = legacyConfiguredExecutablePath();
    struct RegistryView { HKEY hive; REGSAM view; };
    RegistryView const views[] = {
        { HKEY_CURRENT_USER, KEY_WOW64_64KEY }, { HKEY_CURRENT_USER, KEY_WOW64_32KEY },
        { HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY }, { HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY },
    };
    wchar_t const *uninstallPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
    for (RegistryView const &view: views) {
        HKEY base = nullptr;
        if (RegOpenKeyExW(view.hive, uninstallPath, 0, KEY_READ | view.view, &base) != ERROR_SUCCESS)
            continue;
        DWORD index = 0;
        wchar_t subkeyName[512] = {};
        DWORD subkeyLength = 512;
        while (RegEnumKeyExW(base, index++, subkeyName, &subkeyLength, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
            HKEY entry = nullptr;
            if (RegOpenKeyExW(base, subkeyName, 0, KEY_READ | view.view, &entry) == ERROR_SUCCESS) {
                LegacySource source;
                source.type = migration::ESourceType::Installed;
                source.displayName = registryString(entry, L"DisplayName");
                source.publisher = registryString(entry, L"Publisher");
                source.rootPath = registryString(entry, L"InstallLocation");
                source.uninstallCommand = registryString(entry, L"UninstallString");
                source.quietUninstallCommand = registryString(entry, L"QuietUninstallString");
                QString exe = configuredExe;
                if (exe.isEmpty() && !source.rootPath.isEmpty())
                    exe = QDir(source.rootPath).absoluteFilePath("Beeftext.exe");
                if (source.rootPath.isEmpty() && !exe.isEmpty())
                    source.rootPath = QFileInfo(exe).absolutePath();
                source.executablePath = exe;
                source.comboFilePath = configuredCombo;
				bool const recognizable = migration::isRecognizableInstalledCandidate(source.displayName,
					source.publisher, source.rootPath, source.executablePath) && QFileInfo(source.executablePath).isFile();
				source.cleanupSafe = recognizable && !safeRegisteredUninstallCommand(source).isEmpty();
                if (recognizable && QFileInfo(source.comboFilePath).isFile()) {
                    source.comboDigest = fileDigest(source.comboFilePath);
                    source.modified = QFileInfo(source.comboFilePath).lastModified();
                    if (!source.comboDigest.isEmpty())
                        result.append(source);
                }
                RegCloseKey(entry);
            }
            subkeyLength = 512;
        }
        RegCloseKey(base);
    }
    return result;
}


QStringList runningBeeftextExecutables() {
    QStringList result;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return result;
    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (QString::fromWCharArray(entry.szExeFile).compare("Beeftext.exe", Qt::CaseInsensitive) != 0)
                continue;
            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (!process)
                continue;
            std::vector<wchar_t> path(32768, L'\0');
            DWORD length = DWORD(path.size());
            if (QueryFullProcessImageNameW(process, 0, path.data(), &length))
                result.append(QString::fromWCharArray(path.data(), int(length)));
            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}


QString shortcutTarget(QString const &shortcutPath) {
    IShellLinkW *link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
        reinterpret_cast<void **>(&link))))
        return QString();
    IPersistFile *persist = nullptr;
    QString result;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&persist)))) {
        if (SUCCEEDED(persist->Load(reinterpret_cast<LPCWSTR>(shortcutPath.utf16()), STGM_READ))) {
            wchar_t target[32768] = {};
            WIN32_FIND_DATAW data = {};
            if (SUCCEEDED(link->GetPath(target, 32768, &data, SLGP_RAWPATH)))
                result = QString::fromWCharArray(target);
        }
        persist->Release();
    }
    link->Release();
    return QDir::fromNativeSeparators(result);
}


QMap<QString, QStringList> beeftextShortcutTargets() {
    QMap<QString, QStringList> result;
    QStringList roots = {
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation),
    };
    QString const programData = qEnvironmentVariable("PROGRAMDATA");
    if (!programData.isEmpty())
        roots.append(QDir(programData).absoluteFilePath("Microsoft/Windows/Start Menu/Programs"));
    for (QString const &root: roots) {
        if (root.isEmpty() || !QDir(root).exists())
            continue;
        QDirIterator iterator(root, { "*.lnk" }, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            QString const shortcut = iterator.next();
            QString const target = shortcutTarget(shortcut);
            if (QFileInfo(target).fileName().compare("Beeftext.exe", Qt::CaseInsensitive) == 0)
                result[canonicalPath(target)].append(shortcut);
        }
    }
    return result;
}


bool invokeUninstaller(LegacySource const &source) {
	QString const command = safeRegisteredUninstallCommand(source);
	if (command.isEmpty())
		return false;
    QStringList parts = QProcess::splitCommand(command);
    if (parts.isEmpty())
        return false;
    QString const executable = parts.takeFirst();
    QString const parameters = parametersFromCommand(command);
    SHELLEXECUTEINFOW info = {};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = reinterpret_cast<LPCWSTR>(executable.utf16());
    info.lpParameters = reinterpret_cast<LPCWSTR>(parameters.utf16());
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info) || !info.hProcess)
        return false;
    WaitForSingleObject(info.hProcess, INFINITE);
    DWORD exitCode = ERROR_GEN_FAILURE;
    GetExitCodeProcess(info.hProcess, &exitCode);
    CloseHandle(info.hProcess);
    return exitCode == 0;
}


bool recyclePaths(QStringList const &paths) {
    if (paths.isEmpty())
        return true;
    std::wstring list;
    for (QString const &path: paths) {
        std::wstring const item = QDir::toNativeSeparators(path).toStdWString();
        list.append(item);
        list.push_back(L'\0');
    }
    list.push_back(L'\0');
    SHFILEOPSTRUCTW operation = {};
    operation.wFunc = FO_DELETE;
    operation.pFrom = list.c_str();
    operation.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    return SHFileOperationW(&operation) == 0 && !operation.fAnyOperationsAborted;
}


#endif // _WIN32


QStringList protectedCleanupRoots() {
    QStringList result = {
        QDir::homePath(),
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        QCoreApplication::applicationDirPath(),
        qEnvironmentVariable("ProgramFiles"),
        qEnvironmentVariable("ProgramFiles(x86)"),
        qEnvironmentVariable("WINDIR"),
    };
    for (QFileInfo const &drive: QDir::drives())
        result.append(drive.absoluteFilePath());
    return result;
}


void addPortableCandidate(QList<LegacySource> &sources, QString const &executableFolder,
    QStringList const &shortcuts = {}) {
    QString comboPath;
    QString rootPath;
    if (!migration::isStrongPortableCandidate(executableFolder, &comboPath, &rootPath))
        return;
    for (LegacySource &existing: sources) {
        if (existing.type == migration::ESourceType::Portable && samePath(existing.rootPath, rootPath)) {
            existing.shortcutPaths.append(shortcuts);
            existing.shortcutPaths.removeDuplicates();
            return;
        }
    }
    LegacySource source;
    source.type = migration::ESourceType::Portable;
    source.rootPath = rootPath;
    source.executablePath = QDir(executableFolder).absoluteFilePath("Beeftext.exe");
    source.comboFilePath = comboPath;
    source.shortcutPaths = shortcuts;
    source.comboDigest = fileDigest(comboPath);
    source.modified = QFileInfo(comboPath).lastModified();
    source.cleanupSafe = !migration::isBroadCleanupRoot(rootPath, protectedCleanupRoots()) &&
        QFileInfo(rootPath).fileName().contains("beeftext", Qt::CaseInsensitive);
    if (!source.comboDigest.isEmpty())
        sources.append(source);
}


QList<LegacySource> detectSources() {
    QList<LegacySource> sources;
#ifdef _WIN32
    sources = registeredInstalledSources();
    QMap<QString, QStringList> const shortcuts = beeftextShortcutTargets();
    for (auto iterator = shortcuts.constBegin(); iterator != shortcuts.constEnd(); ++iterator)
        addPortableCandidate(sources, QFileInfo(iterator.key()).absolutePath(), iterator.value());
    for (QString const &executable: runningBeeftextExecutables())
        addPortableCandidate(sources, QFileInfo(executable).absolutePath(), shortcuts.value(canonicalPath(executable)));
#endif
    QStringList const shallowRoots = {
        QStandardPaths::writableLocation(QStandardPaths::DesktopLocation),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
    };
    for (QString const &root: shallowRoots) {
        if (root.isEmpty())
            continue;
        addPortableCandidate(sources, root);
        QDir const directory(root);
        for (QFileInfo const &child: directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
            addPortableCandidate(sources, child.absoluteFilePath());
    }
    for (qsizetype i = sources.size() - 1; i >= 0; --i) {
        for (qsizetype j = 0; j < i; ++j) {
            if (sources[i].type == sources[j].type && samePath(sources[i].rootPath, sources[j].rootPath)) {
                sources[j].shortcutPaths.append(sources[i].shortcutPaths);
                sources[j].shortcutPaths.removeDuplicates();
                sources.removeAt(i);
                break;
            }
        }
    }
    return sources;
}


QString sourceTypeName(LegacySource const &source) {
    return source.type == migration::ESourceType::Installed ? QObject::tr("Installed") : QObject::tr("Portable");
}


QJsonObject sourceToJson(LegacySource const &source) {
    QJsonObject object;
    object["type"] = source.type == migration::ESourceType::Installed ? "installed" : "portable";
    object["rootPath"] = source.rootPath;
    object["executablePath"] = source.executablePath;
    object["comboFilePath"] = source.comboFilePath;
    object["displayName"] = source.displayName;
    object["publisher"] = source.publisher;
    object["uninstallCommand"] = source.uninstallCommand;
    object["quietUninstallCommand"] = source.quietUninstallCommand;
    object["shortcutPaths"] = QJsonArray::fromStringList(source.shortcutPaths);
    object["comboDigest"] = QString::fromLatin1(source.comboDigest.toHex());
    object["cleanupSafe"] = source.cleanupSafe;
    return object;
}


LegacySource sourceFromJson(QJsonObject const &object) {
    LegacySource source;
    source.type = object["type"].toString() == "installed" ? migration::ESourceType::Installed : migration::ESourceType::Portable;
    source.rootPath = object["rootPath"].toString();
    source.executablePath = object["executablePath"].toString();
    source.comboFilePath = object["comboFilePath"].toString();
    source.displayName = object["displayName"].toString();
    source.publisher = object["publisher"].toString();
    source.uninstallCommand = object["uninstallCommand"].toString();
    source.quietUninstallCommand = object["quietUninstallCommand"].toString();
    for (QJsonValue const &value: object["shortcutPaths"].toArray())
        source.shortcutPaths.append(value.toString());
    source.comboDigest = QByteArray::fromHex(object["comboDigest"].toString().toLatin1());
    source.cleanupSafe = object["cleanupSafe"].toBool();
    return source;
}


bool writeRecoverySnapshot(LegacySource const &source, QString &outFolder, QString &outError) {
    QString const timestamp = QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmsszzz-'UTC'");
    QString const type = source.type == migration::ESourceType::Installed ? "installed" : "portable";
    outFolder = QDir(globals::migrationBackupFolderPath()).absoluteFilePath(
        QString("%1-%2-%3").arg(timestamp, type, QString::fromLatin1(source.comboDigest.toHex().left(8))));
    if (!QDir().mkpath(outFolder)) {
        outError = QObject::tr("The migration recovery folder could not be created.");
        return false;
    }
    QFile input(source.comboFilePath);
    QSaveFile output(QDir(outFolder).absoluteFilePath(ComboList::defaultFileName));
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) {
        outError = QObject::tr("The original combo library could not be copied to the migration recovery folder.");
        return false;
    }
    QByteArray const contents = input.readAll();
    if (output.write(contents) != contents.size() || !output.commit()) {
        outError = QObject::tr("The migration recovery copy could not be committed.");
        return false;
    }
    QSaveFile metadata(QDir(outFolder).absoluteFilePath("SOURCE.txt"));
    if (!metadata.open(QIODevice::WriteOnly | QIODevice::Text)) {
        outError = QObject::tr("Migration source metadata could not be written.");
        return false;
    }
    QString const text = QString("Source type: %1\nSource folder: %2\nCombo file: %3\nSnapshot UTC: %4\nSHA-256: %5\n")
        .arg(sourceTypeName(source), QDir::toNativeSeparators(source.rootPath),
            QDir::toNativeSeparators(source.comboFilePath), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs),
            QString::fromLatin1(source.comboDigest.toHex()));
    QByteArray const encoded = text.toUtf8();
    if (metadata.write(encoded) != encoded.size() || !metadata.commit()) {
        outError = QObject::tr("Migration source metadata could not be committed.");
        return false;
    }
    return true;
}


bool sourceIsRunning(LegacySource const &source) {
#ifdef _WIN32
    for (QString const &running: runningBeeftextExecutables())
        if (samePath(running, source.executablePath))
            return true;
#else
    Q_UNUSED(source)
#endif
    return false;
}


bool cleanupSource(LegacySource const &source) {
    if (!source.cleanupSafe || fileDigest(source.comboFilePath) != source.comboDigest)
        return false;
#ifdef _WIN32
    if (source.type == migration::ESourceType::Installed) {
		if (safeRegisteredUninstallCommand(source).isEmpty())
            return false;
        return invokeUninstaller(source);
    }
    QString comboPath;
    QString rootPath;
    if (!migration::isStrongPortableCandidate(QFileInfo(source.executablePath).absolutePath(), &comboPath, &rootPath) ||
        !samePath(rootPath, source.rootPath) || migration::isBroadCleanupRoot(rootPath, protectedCleanupRoots()) ||
        !QFileInfo(rootPath).fileName().contains("beeftext", Qt::CaseInsensitive))
        return false;
    QStringList recycle = { rootPath };
    for (QString const &shortcut: source.shortcutPaths) {
        if (QFileInfo(shortcut).isFile() && samePath(shortcutTarget(shortcut), source.executablePath) &&
            !canonicalPath(shortcut).startsWith(canonicalPath(rootPath) + '/', Qt::CaseInsensitive))
            recycle.append(shortcut);
    }
    return recyclePaths(recycle);
#else
    Q_UNUSED(source)
    return false;
#endif
}


bool finishPendingCleanup(QSettings &settings, bool askUser) {
    QByteArray const bytes = settings.value(kPendingCleanupKey).toByteArray();
    QJsonParseError error = {};
    QJsonDocument const document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isArray())
        return false;
    if (askUser && QMessageBox::Yes != QMessageBox::question(nullptr, QObject::tr("Finish Beeftext cleanup"),
        QObject::tr("Your combos were imported and verified, but one or more old Beeftext copies still need cleanup. Retry cleanup now?")))
        return false;
    QJsonArray remaining;
    for (QJsonValue const &value: document.array()) {
        LegacySource const source = sourceFromJson(value.toObject());
        if (sourceIsRunning(source) || !cleanupSource(source))
            remaining.append(value);
    }
    if (!remaining.isEmpty()) {
        settings.setValue(kPendingCleanupKey, QJsonDocument(remaining).toJson(QJsonDocument::Compact));
        settings.sync();
        QMessageBox::warning(nullptr, QObject::tr("Cleanup incomplete"),
            QObject::tr("One or more old Beeftext copies could not be removed safely. Your imported Lean Beeftext combos are intact; cleanup can be retried later."));
        return false;
    }
    settings.remove(kPendingCleanupKey);
    settings.setValue(kMigrationStateKey, int(migration::EState::Complete));
    settings.sync();
    return true;
}


struct MigrationChoice {
    bool accepted { false };
    bool importContent { true };
    bool cleanupInstalled { true };
    bool cleanupPortable { true };
    qsizetype selectedGroup { 0 };
};


MigrationChoice showMigrationDialog(QList<LegacySource> const &sources, QList<QList<qsizetype>> const &contentGroups) {
    QDialog dialog;
    dialog.setWindowTitle(QObject::tr("Import from Beeftext"));
    dialog.setMinimumWidth(620);
    auto *layout = new QVBoxLayout(&dialog);
    auto *heading = new QLabel(QObject::tr("<b>Lean Beeftext found an existing Beeftext setup.</b>"));
    layout->addWidget(heading);

    auto *sourceChoice = new QComboBox;
    for (qsizetype groupIndex = 0; groupIndex < contentGroups.size(); ++groupIndex) {
        QList<qsizetype> const &indices = contentGroups[groupIndex];
        LegacySource const &representative = sources[indices.first()];
        QString text = QString("%1 — %2").arg(sourceTypeName(representative), QDir::toNativeSeparators(representative.rootPath));
        if (indices.size() > 1)
            text += QObject::tr(" (%1 copies with matching combo data)").arg(indices.size());
        sourceChoice->addItem(text, groupIndex);
    }
    if (contentGroups.size() > 1) {
        layout->addWidget(new QLabel(QObject::tr("Different combo libraries were found. Choose the one to import:")));
        layout->addWidget(sourceChoice);
    }

    auto *locations = new QLabel;
    locations->setWordWrap(true);
    layout->addWidget(locations);
    auto *importCheck = new QCheckBox(QObject::tr("Import my combos and groups"));
    importCheck->setChecked(true);
    layout->addWidget(importCheck);
    auto *installedCheck = new QCheckBox(QObject::tr("Remove the old Beeftext installation after import succeeds (Recommended)"));
    auto *portableCheck = new QCheckBox(QObject::tr("Remove the portable Beeftext copy after import succeeds (Recommended)"));
    layout->addWidget(installedCheck);
    layout->addWidget(portableCheck);
    auto *safety = new QLabel(QObject::tr("Nothing will be removed until your combos have been imported and verified."));
    safety->setWordWrap(true);
    layout->addWidget(safety);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QObject::tr("Continue"));
    layout->addWidget(buttons);

    auto update = [&]() {
        qsizetype const group = sourceChoice->currentData().toLongLong();
        bool installed = false;
        bool portable = false;
        QStringList lines;
        for (qsizetype const index: contentGroups[group]) {
            LegacySource const &source = sources[index];
            lines.append(QString("%1: %2").arg(sourceTypeName(source), QDir::toNativeSeparators(source.rootPath)));
			if (source.type == migration::ESourceType::Installed && !source.cleanupSafe)
				lines.append(QObject::tr("Lean Beeftext can import this library, but will not remove the old installation because its registered uninstaller could not be verified."));
            if (source.type == migration::ESourceType::Portable && !source.cleanupSafe)
                lines.append(QObject::tr("This portable copy is in a shared folder, so Lean Beeftext will not remove it automatically."));
            installed |= source.type == migration::ESourceType::Installed && source.cleanupSafe;
            portable |= source.type == migration::ESourceType::Portable && source.cleanupSafe;
        }
        locations->setText(QObject::tr("Detected source locations:<br>%1").arg(lines.join("<br>")));
        installedCheck->setVisible(installed);
        portableCheck->setVisible(portable);
        installedCheck->setChecked(installed && importCheck->isChecked());
        portableCheck->setChecked(portable && importCheck->isChecked());
        installedCheck->setEnabled(importCheck->isChecked());
        portableCheck->setEnabled(importCheck->isChecked());
    };
    QObject::connect(sourceChoice, &QComboBox::currentIndexChanged, &dialog, update);
    QObject::connect(importCheck, &QCheckBox::toggled, &dialog, update);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    update();

    MigrationChoice result;
    result.accepted = dialog.exec() == QDialog::Accepted;
    result.importContent = importCheck->isChecked();
    result.cleanupInstalled = installedCheck->isVisible() && installedCheck->isChecked();
    result.cleanupPortable = portableCheck->isVisible() && portableCheck->isChecked();
    result.selectedGroup = sourceChoice->currentData().toLongLong();
    return result;
}


bool migrateSource(LegacySource const &source, migration::ValidationResult &validation, QString &outError) {
    QString snapshotFolder;
    validation.snapshotCreated = writeRecoverySnapshot(source, snapshotFolder, outError);
    if (!validation.snapshotCreated)
        return false;

    ComboList imported;
    validation.parsed = imported.load(source.comboFilePath, nullptr, &outError);
    if (!validation.parsed)
        return false;
    imported.ensureCorrectGrouping();
    QByteArray const expected = imported.toJsonDocument(true).toJson(QJsonDocument::Compact);

    QString const target = QDir(globals::appDataDir()).absoluteFilePath(ComboList::defaultFileName);
    validation.persisted = imported.save(target, true, &outError);
    if (!validation.persisted)
        return false;

    ComboList reloaded;
    validation.reloaded = reloaded.load(target, nullptr, &outError);
    if (!validation.reloaded)
        return false;
    reloaded.ensureCorrectGrouping();
    QByteArray const actual = reloaded.toJsonDocument(true).toJson(QJsonDocument::Compact);
    validation.corresponds = imported.size() == reloaded.size() &&
        imported.groupListRef().size() == reloaded.groupListRef().size() && expected == actual;
    if (!validation.corresponds)
        outError = QObject::tr("The saved Lean Beeftext combo library did not match the imported source after reload.");
    return validation.corresponds;
}


} // namespace


void LegacyMigrationManager::runIfNeeded() {
    if (isInPortableMode())
        return;
    QSettings &settings = PreferencesManager::instance().settings();
    migration::EState const state = migration::EState(settings.value(kMigrationStateKey, int(migration::EState::NeverChecked)).toInt());
    if (state == migration::EState::ImportCompleted) {
        finishPendingCleanup(settings, true);
        return;
    }
    QString const leanComboPath = QDir(globals::appDataDir()).absoluteFilePath(ComboList::defaultFileName);
    if (!migration::shouldRunMigration(false, QFileInfo(leanComboPath).exists(), state)) {
        if (state == migration::EState::NeverChecked && QFileInfo(leanComboPath).exists()) {
            settings.setValue(kMigrationStateKey, int(migration::EState::Complete));
            settings.sync();
        }
        return;
    }

    QList<LegacySource> const sources = detectSources();
    if (sources.isEmpty()) {
        settings.setValue(kMigrationStateKey, int(migration::EState::Complete));
        settings.sync();
        return;
    }
    QList<QByteArray> digests;
    for (LegacySource const &source: sources)
        digests.append(source.comboDigest);
    QList<QList<qsizetype>> const contentGroups = migration::groupSourcesByContent(digests);
    MigrationChoice const choice = showMigrationDialog(sources, contentGroups);
    if (!choice.accepted)
        return;
    if (!choice.importContent) {
        settings.setValue(kMigrationStateKey, int(migration::EState::Complete));
        settings.sync();
        return;
    }

    QList<qsizetype> const selectedIndices = contentGroups[choice.selectedGroup];
    LegacySource const &selected = sources[selectedIndices.first()];
    for (qsizetype const index: selectedIndices) {
        if (sourceIsRunning(sources[index])) {
            QMessageBox::warning(nullptr, QObject::tr("Close Beeftext"),
                QObject::tr("Close the detected upstream Beeftext application, then start Lean Beeftext again. Nothing has been imported or removed."));
            return;
        }
    }

    migration::ValidationResult validation;
    QString error;
    if (!migrateSource(selected, validation, error) || !migration::cleanupAllowed(validation)) {
        QMessageBox::critical(nullptr, QObject::tr("Import not completed"),
            QObject::tr("Lean Beeftext could not safely import and verify the combo library. The old Beeftext copy was not changed.\n\n%1").arg(error));
        return;
    }

    QJsonArray pending;
    for (qsizetype const index: selectedIndices) {
        LegacySource const &source = sources[index];
        bool const selectedForCleanup = source.type == migration::ESourceType::Installed ?
            choice.cleanupInstalled : choice.cleanupPortable;
        if (selectedForCleanup && source.cleanupSafe) {
            if (index == selectedIndices.first()) {
                pending.append(sourceToJson(source));
            } else {
                QString additionalSnapshot;
                QString snapshotError;
                if (writeRecoverySnapshot(source, additionalSnapshot, snapshotError))
                    pending.append(sourceToJson(source));
                else
                    QMessageBox::warning(nullptr, QObject::tr("Cleanup skipped"),
                        QObject::tr("A recovery copy could not be made for %1, so that old copy will not be removed.\n\n%2")
                            .arg(QDir::toNativeSeparators(source.rootPath), snapshotError));
            }
        }
    }
    settings.setValue(kMigrationStateKey, int(migration::EState::ImportCompleted));
    settings.setValue(kPendingCleanupKey, QJsonDocument(pending).toJson(QJsonDocument::Compact));
    settings.sync();
    if (pending.isEmpty()) {
        settings.remove(kPendingCleanupKey);
        settings.setValue(kMigrationStateKey, int(migration::EState::Complete));
        settings.sync();
    } else {
        finishPendingCleanup(settings, false);
    }
}
