/// \file
/// \author Xavier Michelon
///
/// \brief Implementation of combo import dialog
///  
/// Copyright (c) Xavier Michelon. All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.  


#include "stdafx.h"
#include "ComboImportDialog.h"
#include "ComboManager.h"
#include "ComboPortability.h"
#include "Preferences/PreferencesManager.h"
#include "BeeftextGlobals.h"
#include <XMiLib/Exception.h>
#include <XMiLib/XMiLibConstants.h>


//****************************************************************************************************************************************************
/// \param[in] filePath The path of the file to load.
/// \param[out] outResult The loaded combos
///
//****************************************************************************************************************************************************
bool loadCombosFromCsvFile(QString const &filePath, ComboList &outResult) {
    outResult.clear();
    QVector<QStringList> csvData;
	if (!combo_portability::loadLegacyCsvRows(filePath, csvData))
        return false;
    for (QStringList const &row: csvData) {
        if (row.size() != 3)
            continue;
        QString const name = row[2].trimmed();
        QString const keyword = row[0];
        QString const snippet = row[1];
        SpCombo const combo = std::make_shared<Combo>(name.isEmpty() ? keyword : name, keyword, snippet, QString(), EMatchingMode::Default, ECaseSensitivity::Default, true);
        if (combo->isValid())
            outResult.push_back(combo);
    }
    return true;
}


//****************************************************************************************************************************************************
/// \param[in] filePath The path of the file to import. Can be null.
/// \param[in] group The group to select in the group combo. Can be null.
/// \param[in] parent The parent widget of the dialog
//****************************************************************************************************************************************************
ComboImportDialog::ComboImportDialog(QString const &filePath, SpGroup const &group, QWidget *parent)
    : QDialog(parent, xmilib::constants::kDefaultDialogFlags), ui_() {
    ui_.setupUi(this);
	setWindowTitle(tr("Import Combos"));

    connect(ui_.buttonBrowse, &QPushButton::clicked, this, &ComboImportDialog::onBrowse);
    connect(ui_.buttonCancel, &QPushButton::clicked, this, &ComboImportDialog::onCancel);
    connect(ui_.buttonImport, &QPushButton::clicked, this, &ComboImportDialog::onImport);
    connect(ui_.editPath, &QLineEdit::textChanged, this, &ComboImportDialog::onEditPathTextChanged);
    connect(ui_.radioImportNewer, &QRadioButton::toggled, this, &ComboImportDialog::onConflictRadioToggled);
    connect(ui_.radioOverwrite, &QRadioButton::toggled, this, &ComboImportDialog::onConflictRadioToggled);
    connect(ui_.radioSkipConflicts, &QRadioButton::toggled, this, &ComboImportDialog::onConflictRadioToggled);

    GroupList &groupList = ComboManager::instance().groupListRef();
    groupList.ensureNotEmpty();
    ui_.comboGroup->setContent(groupList);
    if (group)
        ui_.comboGroup->setCurrentGroup(group);
    if (!filePath.isEmpty()) {
        ui_.editPath->setText(QDir::toNativeSeparators(filePath));
        if (!filePath.isEmpty())
            PreferencesManager::instance().setLastComboImportExportPath(filePath);
    } else
        this->updateGui();
}


//****************************************************************************************************************************************************
/// \param[in] event The event
//****************************************************************************************************************************************************
void ComboImportDialog::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) // we only accept files
        event->acceptProposedAction();
}


//****************************************************************************************************************************************************
/// \param[in] event The event
//****************************************************************************************************************************************************
void ComboImportDialog::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasUrls()) // we only accept files
        event->acceptProposedAction();
}


//****************************************************************************************************************************************************
/// \param[in] event The event
//****************************************************************************************************************************************************
void ComboImportDialog::dragLeaveEvent(QDragLeaveEvent *event) {
    event->accept();
}


//****************************************************************************************************************************************************
/// \param[in] event The event
//****************************************************************************************************************************************************
void ComboImportDialog::dropEvent(QDropEvent *event) {
    QMimeData const *mimeData = event->mimeData();
    if (!mimeData->hasUrls())
        return;
    QList<QUrl> urls = mimeData->urls();
    if (!urls.empty()) // should always be the case
    {
        QString const &path = urls[0].toLocalFile();
        ui_.editPath->setText(QDir::toNativeSeparators(path));
    }
    event->acceptProposedAction();
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboImportDialog::updateGui() const {
	ui_.label->setText(preserveImportedGroups_
		? tr("Groups from this Lean file will be preserved.") : tr("Import into group"));
	ui_.comboGroup->setVisible(!preserveImportedGroups_);
    qint32 const conflictingNewerCount = conflictingNewerCombos_.size();
    qint32 const conflictingTotalCount = conflictingNewerCount + conflictingOlderCombos_.size();

    ui_.groupBoxConflicts->setVisible(conflictingTotalCount > 0);

    if (conflictingTotalCount) {
        ui_.radioSkipConflicts->setText(conflictingTotalCount > 1 ? tr("Skip %1 conflicting combos.")
            .arg(conflictingTotalCount) : tr("Skip 1 conflicting combo."));
        ui_.radioOverwrite->setText(conflictingTotalCount > 1 ? tr("Overwrite %1 conflicting combos.")
            .arg(conflictingTotalCount) : tr("Overwrite 1 conflicting combo."));
        ui_.radioImportNewer->setVisible(conflictingNewerCount);
        if (conflictingNewerCount)
            ui_.radioImportNewer->setText(conflictingNewerCount > 1 ? tr("Overwrite %1 older conflicting combos.")
                .arg(conflictingNewerCount) : tr("Overwrite 1 older conflicting combo."));
        if ((!conflictingNewerCombos_.size()) && ui_.radioImportNewer->isChecked()) {
            QSignalBlocker b1(ui_.radioImportNewer), b2(ui_.radioSkipConflicts);
            ui_.radioSkipConflicts->setChecked(true);
        }
    }

    if (!currentError_.isEmpty()) {
        ui_.labelImportCount->setText(currentError_);
        ui_.labelImportCount->setVisible(true);
        return;
    }

    qint32 const importCount = this->computeTotalImportCount();
    ui_.buttonImport->setEnabled(importCount);
    ui_.labelImportCount->setVisible(importCount);
    if (importCount)
        ui_.labelImportCount->setText(importCount > 1 ? tr("%1 combos will be imported.").arg(importCount) :
                                      tr("1 combo will be imported."));
}


//****************************************************************************************************************************************************
/// \return The number of combos that will be imported based on the user choices
//****************************************************************************************************************************************************
qint32 ComboImportDialog::computeTotalImportCount() const {
    qint32 const result = importableCombos_.size();
    if (ui_.radioImportNewer->isChecked())
        return result + conflictingNewerCombos_.size();
    if (ui_.radioOverwrite->isChecked())
        return result + conflictingNewerCombos_.size() + conflictingOlderCombos_.size();
    return result;
}


//****************************************************************************************************************************************************
/// \param[out] outFailureCount on exit, this variable contains the number of failed imports
//****************************************************************************************************************************************************
void ComboImportDialog::performFinalImport(qint32 &outFailureCount) {
    outFailureCount = 0;
    ComboList &comboList = ComboManager::instance().comboListRef();
    qint32 failureCount = 0;
	SpGroup const destinationGroup = ui_.comboGroup->currentGroup();
	QHash<QUuid, SpGroup> importedGroupMap;
	if (preserveImportedGroups_) {
		GroupList &existingGroups = ComboManager::instance().groupListRef();
		for (SpGroup const &importedGroup: importedGroups_) {
			SpGroup resolvedGroup;
			GroupList::iterator const uuidMatch = existingGroups.findByUuid(importedGroup->uuid());
			if (uuidMatch != existingGroups.end())
				resolvedGroup = *uuidMatch;
			if (!resolvedGroup) {
				GroupList::iterator const nameMatch = std::find_if(existingGroups.begin(), existingGroups.end(),
					[&](SpGroup const &group) { return group && group->name() == importedGroup->name(); });
				if (nameMatch != existingGroups.end())
					resolvedGroup = *nameMatch;
			}
			if (!resolvedGroup) {
				if (!existingGroups.append(importedGroup))
					throw xmilib::Exception(tr("A group from the Lean combo file could not be imported."));
				resolvedGroup = importedGroup;
			}
			importedGroupMap.insert(importedGroup->uuid(), resolvedGroup);
		}
	} else if (!destinationGroup) {
		throw xmilib::Exception(tr("Please select a valid group."));
	}

	auto assignGroup = [&](SpCombo const &combo) {
		if (!preserveImportedGroups_) {
			combo->setGroup(destinationGroup);
			return;
		}
		SpGroup const importedGroup = combo->group();
		if (!importedGroup || !importedGroupMap.contains(importedGroup->uuid()))
			throw xmilib::Exception(tr("A combo's group could not be restored from the Lean combo file."));
		combo->setGroup(importedGroupMap.value(importedGroup->uuid()));
	};
    for (SpCombo const &combo: importableCombos_) {
		assignGroup(combo);
        if (!comboList.append(combo))
            ++failureCount;
    }
    if (ui_.radioSkipConflicts->isChecked())
        return;

    if (ui_.radioOverwrite->isChecked() && conflictingOlderCombos_.size())
        std::copy(conflictingOlderCombos_.begin(), conflictingOlderCombos_.end(), std::back_inserter(conflictingNewerCombos_));

    for (SpCombo const &combo: conflictingNewerCombos_) {
        ComboList::iterator const it = comboList.findByKeyword(combo->keyword());
        if (it == comboList.end()) {
            ++failureCount;
            continue;
        }
		assignGroup(combo);
        *it = combo;
    }
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void ComboImportDialog::onImport() {
    try {
        if (!this->computeTotalImportCount())
            return;

        qint32 failureCount = 0;
        this->performFinalImport(failureCount);

        QString errorMsg;
        if ((!ComboManager::instance().saveComboListToFile(&errorMsg)))
            QMessageBox::critical(this, tr("Error"), errorMsg);

        if (failureCount) {
            globals::debugLog().addError(QString("%1 supposedly possible combo import failed").arg(failureCount));
            QMessageBox::critical(this, tr("Error"), (failureCount > 1) ?
                                                     tr("%1 combos could not be imported.").arg(failureCount) : tr("A combo could not be imported."));
        }

        this->accept();
    }
    catch (xmilib::Exception const &e) {
        QMessageBox::critical(this, tr("&Error"), e.qwhat());
    }
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void ComboImportDialog::onCancel() {
    this->reject();
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboImportDialog::onBrowse() {
    PreferencesManager const &prefs = PreferencesManager::instance();
	QString const path = QFileDialog::getOpenFileName(this, tr("Import Combos"), prefs.lastComboImportExportPath(), combo_portability::importFileDialogFilter());
    if (path.isEmpty())
        return;
    prefs.setLastComboImportExportPath(path);
    ui_.editPath->setText(QDir::toNativeSeparators(path));
}


//****************************************************************************************************************************************************
/// \param[in] text The new text of the edit control
//****************************************************************************************************************************************************
void ComboImportDialog::onEditPathTextChanged(QString const &text) {
    currentError_ = QString();
    importableCombos_.clear();
    conflictingOlderCombos_.clear();
    conflictingNewerCombos_.clear();
	importedGroups_.clear();
	preserveImportedGroups_ = false;

    ComboList candidateList;
    ComboList &comboList = ComboManager::instance().comboListRef();
    QString const path = QDir::fromNativeSeparators(text);
	combo_portability::EInputFormat const format = combo_portability::inputFormat(path);
	if (format == combo_portability::EInputFormat::LeanTextJson
		|| format == combo_portability::EInputFormat::LegacyJson) {
		QJsonDocument document;
		QString errorMessage;
		if (!combo_portability::loadJsonForImport(path, ComboList::fileFormatVersionNumber, document,
			preserveImportedGroups_, &errorMessage)
			|| !candidateList.readFromJsonDocument(document, nullptr, &errorMessage)) {
			currentError_ = errorMessage.isEmpty() ? tr("The file is invalid.") : errorMessage;
		}
		if (currentError_.isEmpty() && preserveImportedGroups_)
			importedGroups_ = candidateList.groupListRef();
	} else if (format == combo_portability::EInputFormat::LegacyCsv) {
		if (!loadCombosFromCsvFile(path, candidateList))
			currentError_ = tr("The legacy Beeftext CSV file is invalid.");
	} else {
		currentError_ = tr("The selected file type is not supported. Choose a Lean .txt, Beeftext .json, or Beeftext .csv file.");
	}
	if (currentError_.isEmpty() && candidateList.isEmpty())
		currentError_ = tr("The file does not contain importable combo data.");
	if (!currentError_.isEmpty()) {
		this->updateGui();
		return;
    }

    for (SpCombo const &combo: candidateList) {
		// Legacy imports keep the established behavior of receiving new UUIDs. Lean bundles
		// retain UUIDs for a faithful transfer unless that UUID already identifies a
		// different local combo, in which case a new UUID prevents a duplicate-ID failure.
		ComboList::const_iterator const uuidMatch = comboList.findByUuid(combo->uuid());
		if (!preserveImportedGroups_ || (uuidMatch != comboList.end() && (*uuidMatch)->keyword() != combo->keyword()))
			combo->changeUuid();
        ComboList::const_iterator const it = comboList.findByKeyword(combo->keyword());
        if (comboList.end() == it) {
            importableCombos_.append(combo);
            continue;
        }
        Combo const &existingCombo = **it;
        if (combo->modificationDateTime() > existingCombo.modificationDateTime())
            conflictingNewerCombos_.append(combo);
        else
            conflictingOlderCombos_.append(combo);
    }
    this->updateGui();
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboImportDialog::onConflictRadioToggled(bool state) const {
    if (!state)
        return; // we are only interested in signals from the radio being checked
    this->updateGui();
}
