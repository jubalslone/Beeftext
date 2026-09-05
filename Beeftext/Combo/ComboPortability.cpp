/// \file
///
/// \brief Lean Beeftext combo import/export format helpers.


#include "ComboPortability.h"

#include <XMiLib/File/CsvIO.h>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonParseError>
#include <QObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>


namespace {


QString const kFormat = "lean-beeftext-combos";
qint32 constexpr kVersion = 1;
QString const kKeyFormat = "format";
QString const kKeyVersion = "version";
QString const kKeyGroups = "groups";
QString const kKeyCombos = "combos";
QString const kKeyUuid = "uuid";
QString const kKeyGroup = "group";
QString const kKeyInternalVersion = "fileFormatVersion";


bool fail(QString const &message, QString *outErrorMessage) {
	if (outErrorMessage)
		*outErrorMessage = message;
	return false;
}


bool validateObjectArray(QJsonValue const &value, QString const &name, QJsonArray &outArray,
	QString *outErrorMessage) {
	if (!value.isArray())
		return fail(QString("The Lean combo file has an invalid '%1' list.").arg(name), outErrorMessage);
	outArray = value.toArray();
	for (QJsonValue const &entry: outArray) {
		if (!entry.isObject())
			return fail(QString("The Lean combo file contains an invalid entry in '%1'.").arg(name), outErrorMessage);
	}
	return true;
}


} // anonymous namespace


namespace combo_portability {


QString defaultExportFileName() {
	return "Lean-Beeftext-Combos.txt";
}


QString exportFileDialogFilter() {
	return QObject::tr("Lean Beeftext combo files (*.txt)");
}


QString importFileDialogFilter() {
	return QObject::tr("Lean Beeftext combo files (*.txt);;Supported combo files (*.txt *.json *.csv);;"
		"Legacy Beeftext JSON files (*.json);;Legacy Beeftext CSV files (*.csv);;All files (*.*)");
}


EInputFormat inputFormat(QString const &path) {
	QString const suffix = QFileInfo(path).suffix();
	if (suffix.compare("txt", Qt::CaseInsensitive) == 0)
		return EInputFormat::LeanTextJson;
	if (suffix.compare("json", Qt::CaseInsensitive) == 0)
		return EInputFormat::LegacyJson;
	if (suffix.compare("csv", Qt::CaseInsensitive) == 0)
		return EInputFormat::LegacyCsv;
	return EInputFormat::Unsupported;
}


QJsonArray referencedGroups(QJsonArray const &combos, QJsonArray const &groups) {
	QSet<QString> referencedUuids;
	for (QJsonValue const &value: combos) {
		if (value.isObject())
			referencedUuids.insert(value.toObject().value(kKeyGroup).toString());
	}

	QJsonArray result;
	for (QJsonValue const &value: groups) {
		if (value.isObject() && referencedUuids.contains(value.toObject().value(kKeyUuid).toString()))
			result.append(value);
	}
	return result;
}


QJsonDocument createLeanBundle(QJsonArray const &combos, QJsonArray const &groups) {
	QJsonObject root;
	root.insert(kKeyFormat, kFormat);
	root.insert(kKeyVersion, kVersion);
	root.insert(kKeyGroups, groups);
	root.insert(kKeyCombos, combos);
	return QJsonDocument(root);
}


bool saveLeanBundle(QString const &path, QJsonArray const &combos, QJsonArray const &groups,
	QString *outErrorMessage) {
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly))
		return fail(QObject::tr("The combo export file could not be opened for writing."), outErrorMessage);
	QByteArray const data = createLeanBundle(combos, groups).toJson(QJsonDocument::Indented);
	if (file.write(data) != data.size())
		return fail(QObject::tr("The combo export file could not be written completely."), outErrorMessage);
	if (!file.commit())
		return fail(QObject::tr("The combo export file could not be saved."), outErrorMessage);
	return true;
}


bool readLeanBundle(QByteArray const &data, qint32 internalFormatVersion, QJsonDocument &outDocument,
	QString *outErrorMessage) {
	QJsonParseError parseError;
	QJsonDocument const bundle = QJsonDocument::fromJson(data, &parseError);
	if (parseError.error != QJsonParseError::NoError)
		return fail(QObject::tr("The Lean combo file is malformed JSON: %1").arg(parseError.errorString()), outErrorMessage);
	if (!bundle.isObject())
		return fail(QObject::tr("The Lean combo file must contain a JSON object."), outErrorMessage);

	QJsonObject const root = bundle.object();
	QSet<QString> const expectedKeys = { kKeyFormat, kKeyVersion, kKeyGroups, kKeyCombos };
	QSet<QString> actualKeys;
	for (QString const &key: root.keys())
		actualKeys.insert(key);
	if (actualKeys != expectedKeys)
		return fail(QObject::tr("The Lean combo file contains missing or unsupported top-level data."), outErrorMessage);
	if (root.value(kKeyFormat).toString() != kFormat)
		return fail(QObject::tr("The file is not a Lean Beeftext combo export."), outErrorMessage);
	QJsonValue const versionValue = root.value(kKeyVersion);
	if (!versionValue.isDouble() || versionValue.toDouble() != kVersion)
		return fail(QObject::tr("This Lean combo export version is not supported."), outErrorMessage);

	QJsonArray groups;
	QJsonArray combos;
	if (!validateObjectArray(root.value(kKeyGroups), kKeyGroups, groups, outErrorMessage)
		|| !validateObjectArray(root.value(kKeyCombos), kKeyCombos, combos, outErrorMessage))
		return false;

	QSet<QString> groupUuids;
	for (QJsonValue const &value: groups) {
		QString const uuidText = value.toObject().value(kKeyUuid).toString();
		if (QUuid(uuidText).isNull() || groupUuids.contains(uuidText))
			return fail(QObject::tr("The Lean combo file contains an invalid or duplicate group identifier."), outErrorMessage);
		groupUuids.insert(uuidText);
	}
	for (QJsonValue const &value: combos) {
		QString const groupUuid = value.toObject().value(kKeyGroup).toString();
		if (!groupUuid.isEmpty() && !groupUuids.contains(groupUuid))
			return fail(QObject::tr("A combo refers to a group that is missing from the Lean combo file."), outErrorMessage);
	}

	QJsonObject internalRoot;
	internalRoot.insert(kKeyInternalVersion, internalFormatVersion);
	internalRoot.insert(kKeyGroups, groups);
	internalRoot.insert(kKeyCombos, combos);
	outDocument = QJsonDocument(internalRoot);
	return true;
}


bool loadJsonForImport(QString const &path, qint32 internalFormatVersion, QJsonDocument &outDocument,
	bool &outPreserveGroups, QString *outErrorMessage) {
	EInputFormat const format = inputFormat(path);
	if (format != EInputFormat::LeanTextJson && format != EInputFormat::LegacyJson)
		return fail(QObject::tr("The selected file type is not supported."), outErrorMessage);

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return fail(QObject::tr("The selected combo file could not be opened."), outErrorMessage);
	QByteArray const data = file.readAll();
	QJsonDocument candidate;
	bool preserveGroups = false;
	if (format == EInputFormat::LeanTextJson) {
		if (!readLeanBundle(data, internalFormatVersion, candidate, outErrorMessage))
			return false;
		preserveGroups = !candidate.object().value(kKeyGroups).toArray().isEmpty();
	} else {
		QJsonParseError parseError;
		candidate = QJsonDocument::fromJson(data, &parseError);
		if (parseError.error != QJsonParseError::NoError)
			return fail(QObject::tr("The legacy Beeftext JSON file is malformed: %1").arg(parseError.errorString()), outErrorMessage);
	}
	outDocument = candidate;
	outPreserveGroups = preserveGroups;
	return true;
}


bool loadLegacyCsvRows(QString const &path, QVector<QStringList> &outRows, QString *outErrorMessage) {
	QVector<QStringList> rows;
	if (!xmilib::loadCsvFile(path, rows))
		return fail(QObject::tr("The legacy Beeftext CSV file could not be read."), outErrorMessage);
	outRows = rows;
	return true;
}


} // namespace combo_portability
