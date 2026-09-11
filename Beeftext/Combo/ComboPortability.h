/// \file
///
/// \brief Lean Beeftext combo import/export format helpers.


#ifndef BEEFTEXT_COMBO_PORTABILITY_H
#define BEEFTEXT_COMBO_PORTABILITY_H


#include <QJsonArray>
#include <QJsonDocument>
#include <QString>
#include <QStringList>
#include <QVector>


namespace combo_portability {


enum class EInputFormat {
	LeanTextJson,
	LegacyJson,
	LegacyCsv,
	Unsupported,
};


QString defaultExportFileName(); ///< Return the default Lean combo-export filename.
QString exportFileDialogFilter(); ///< Return the Lean combo-export file picker filter.
QString importFileDialogFilter(); ///< Return the combo-import file picker filter.
EInputFormat inputFormat(QString const &path); ///< Classify an import file by its extension.
QJsonArray referencedGroups(QJsonArray const &combos, QJsonArray const &groups); ///< Return the groups referenced by the supplied combos.
QJsonDocument createLeanBundle(QJsonArray const &combos, QJsonArray const &groups); ///< Build a Lean combo bundle.
bool saveLeanBundle(QString const &path, QJsonArray const &combos, QJsonArray const &groups,
	QString *outErrorMessage = nullptr); ///< Save a Lean combo bundle as UTF-8 JSON.
bool readLeanBundle(QByteArray const &data, qint32 internalFormatVersion, QJsonDocument &outDocument,
	QString *outErrorMessage = nullptr); ///< Validate and unwrap a Lean combo bundle.
bool loadJsonForImport(QString const &path, qint32 internalFormatVersion, QJsonDocument &outDocument,
	bool &outPreserveGroups, QString *outErrorMessage = nullptr); ///< Load Lean or legacy JSON import data.
bool loadLegacyCsvRows(QString const &path, QVector<QStringList> &outRows,
	QString *outErrorMessage = nullptr); ///< Load an upstream-compatible CSV combo export.


} // namespace combo_portability


#endif // BEEFTEXT_COMBO_PORTABILITY_H
