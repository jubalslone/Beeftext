/// \file
/// \brief Testable safety rules for one-time compatible Beeftext migration.


#include "LegacyMigrationCore.h"
#include <QDir>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>


namespace migration {


namespace {


QString normalizedPath(QString const &path) {
	if (path.trimmed().isEmpty())
		return QString();
    QFileInfo const info(path);
    QString result = info.canonicalFilePath();
    if (result.isEmpty())
        result = info.absoluteFilePath();
    return QDir::cleanPath(QDir::fromNativeSeparators(result)).toCaseFolded();
}


QString executableFromCommand(QString const &command) {
    QStringList const parts = QProcess::splitCommand(command.trimmed());
    return parts.isEmpty() ? QString() : parts.first();
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


} // namespace


bool isStrongPortableCandidate(QString const &executableFolder, QString *outComboFilePath,
    QString *outDedicatedRootPath, EPortableProduct *outProduct) {
    QDir const appDir(executableFolder);
    QString const upstreamExecutable = appDir.absoluteFilePath("Beeftext.exe");
    QString const leanExecutable = appDir.absoluteFilePath("LeanBeeftext.exe");
    bool const hasUpstreamExecutable = QFileInfo(upstreamExecutable).isFile();
    bool const hasLeanExecutable = QFileInfo(leanExecutable).isFile();
    if (hasUpstreamExecutable == hasLeanExecutable)
        return false;
    EPortableProduct const product = hasLeanExecutable ? EPortableProduct::LeanBeeftext : EPortableProduct::UpstreamBeeftext;
    QString comboPath;
    QString rootPath;
    if (QFileInfo(appDir.absoluteFilePath("Portable.bin")).isFile()) {
        comboPath = appDir.absoluteFilePath("Data/comboList.json");
        rootPath = appDir.absolutePath();
    } else if (QFileInfo(appDir.absoluteFilePath("PortableApps.bin")).isFile()) {
        comboPath = appDir.absoluteFilePath("../../Data/settings/comboList.json");
        rootPath = appDir.absoluteFilePath("../..");
    } else {
        return false;
    }

    QFile comboFile(comboPath);
    if (!QFileInfo(comboPath).isFile() || !comboFile.open(QIODevice::ReadOnly))
        return false;
    QByteArray const comboData = comboFile.readAll().trimmed();
    if (comboData.isEmpty())
        return false;
    if (product == EPortableProduct::LeanBeeftext) {
        QJsonParseError error;
        QJsonDocument const document = QJsonDocument::fromJson(comboData, &error);
        QJsonObject const root = document.object();
        if (error.error != QJsonParseError::NoError || !document.isObject() ||
            !root.value("fileFormatVersion").isDouble() || !root.value("combos").isArray() ||
            (root.value("fileFormatVersion").toInt() >= 3 && !root.value("groups").isArray()))
            return false;
    }

    if (outComboFilePath)
        *outComboFilePath = QFileInfo(comboPath).absoluteFilePath();
    if (outDedicatedRootPath)
        *outDedicatedRootPath = QFileInfo(rootPath).absoluteFilePath();
    if (outProduct)
        *outProduct = product;
    return true;
}


bool isBroadCleanupRoot(QString const &candidateRoot, QStringList const &protectedRoots) {
    QString const candidate = normalizedPath(candidateRoot);
    if (candidate.isEmpty())
        return true;
    QDir const candidateDir(candidateRoot);
    if (candidateDir.isRoot())
        return true;
    for (QString const &root: protectedRoots)
        if (!root.isEmpty() && candidate == normalizedPath(root))
            return true;
    return false;
}


bool isRecognizableInstalledCandidate(QString const &displayName, QString const &publisher,
	QString const &installLocation, QString const &executablePath) {
    QString const foldedName = displayName.toCaseFolded();
    if (!foldedName.contains("beeftext") || foldedName.contains("lean beeftext"))
        return false;
    if (!publisher.trimmed().isEmpty() && !publisher.contains("Michelon", Qt::CaseInsensitive) &&
        !publisher.contains("Beeftext", Qt::CaseInsensitive))
        return false;

    QString const root = normalizedPath(installLocation);
    QString const executable = normalizedPath(executablePath);
	if (root.isEmpty() || executable.isEmpty())
        return false;
    QString const prefix = root.endsWith('/') ? root : root + '/';
	return executable.startsWith(prefix) &&
		QFileInfo(executablePath).fileName().compare("Beeftext.exe", Qt::CaseInsensitive) == 0;
}


bool installedMetadataIsConsistent(QString const &displayName, QString const &publisher,
    QString const &installLocation, QString const &uninstallCommand, QString const &executablePath) {
	if (!isRecognizableInstalledCandidate(displayName, publisher, installLocation, executablePath))
		return false;
	QString const root = normalizedPath(installLocation);
	QString const uninstaller = normalizedPath(executableFromCommand(uninstallCommand));
	if (uninstaller.isEmpty())
		return false;
	QString const prefix = root.endsWith('/') ? root : root + '/';
	if (!uninstaller.startsWith(prefix))
        return false;
    QString const uninstallName = QFileInfo(uninstaller).fileName().toCaseFolded();
	return uninstallName.startsWith("unins") || uninstallName.contains("uninstall");
}


bool buildVerifiedUpstreamNsisUninstallParameters(QString const &displayName, QString const &publisher,
	QString const &installLocation, QString const &uninstallCommand, QString const &executablePath,
	QString *outParameters) {
	if (!outParameters || !installedMetadataIsConsistent(displayName, publisher, installLocation,
		uninstallCommand, executablePath))
		return false;
	if (displayName.trimmed().compare("Beeftext", Qt::CaseInsensitive) != 0 ||
		(publisher.trimmed().compare("beeftext.org", Qt::CaseInsensitive) != 0 &&
			!publisher.contains("Michelon", Qt::CaseInsensitive)))
		return false;
	QString const uninstaller = executableFromCommand(uninstallCommand);
	if (QFileInfo(uninstaller).fileName().compare("Uninstall.exe", Qt::CaseInsensitive) != 0)
		return false;
	QString parameters = parametersFromCommand(uninstallCommand);
	if (parameters.contains(QRegularExpression("(?:^|\\s)_\\?=", QRegularExpression::CaseInsensitiveOption)))
		return false; // Never trust or duplicate a registered install-root override.
	if (!parameters.isEmpty())
		parameters.append(' ');
	// NSIS requires _?= to be the final argument and its path to remain unquoted, including when it contains spaces.
	parameters.append("_?=" + QDir::toNativeSeparators(QDir::cleanPath(installLocation)));
	*outParameters = parameters;
	return true;
}


QByteArray sourceContentDigest(QByteArray const &contents) {
	return QCryptographicHash::hash(contents, QCryptographicHash::Sha256);
}


bool sourceContentMatchesDigest(QByteArray const &contents, QByteArray const &digest) {
	return !digest.isEmpty() && sourceContentDigest(contents) == digest;
}


bool installedCleanupPostconditionsMet(bool executableExists, bool uninstallRegistrationExists) {
	return !executableExists && !uninstallRegistrationExists;
}


QList<QList<qsizetype>> groupSourcesByContent(QList<QByteArray> const &digests) {
    QList<QList<qsizetype>> result;
    QHash<QByteArray, qsizetype> groupByDigest;
    for (qsizetype i = 0; i < digests.size(); ++i) {
        auto const existing = groupByDigest.constFind(digests[i]);
        if (existing == groupByDigest.constEnd()) {
            groupByDigest.insert(digests[i], result.size());
            result.append({ i });
        } else {
            result[*existing].append(i);
        }
    }
    return result;
}


bool cleanupAllowed(ValidationResult const &result) {
    return result.snapshotCreated && result.parsed && result.persisted && result.reloaded && result.corresponds;
}


bool shouldRunMigration(bool portableMode, bool leanComboFileExists, EState state) {
    return !portableMode && !leanComboFileExists && state == EState::NeverChecked;
}


} // namespace migration
