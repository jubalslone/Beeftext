/// \file
/// \author Xavier Michelon
///
/// \brief Declaration of combo table frame
///  
/// Copyright (c) Xavier Michelon. All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.  


#include "stdafx.h"
#include "ComboTableWidget.h"
#include "ComboImportDialog.h"
#include "ComboExportDialog.h"
#include "ComboPortability.h"
#include "ComboManager.h"
#include "ComboDialog.h"
#include "Preferences/PreferencesManager.h"
#include "BeeftextConstants.h"
#include "BeeftextGlobals.h"
#include <XMiLib/Exception.h>


namespace {


qint32 constexpr kMinNameColumnDefaultWidth = 200; ///< The maximum default width in pixels of the name column
qint32 constexpr kMaxNameColumnDefaultWidth = 500; ///< The maximum default width in pixels of the name column
qint32 constexpr kMaxSnippetColumnWDefaultWith = 600; ///< The maximum default width in pixels of the snippet column
char const *kPropMoveToMenu("moveMenu"); ///< The matching menu property

QString moveMenuTitle() { return QObject::tr("Move To", "Move entry in the Combo context menu"); }

struct ActionMapping {
    QAction **action { nullptr };
    QString text;
    QString tooltip;
    QString shortcut;
    void (ComboTableWidget::*slot)() { nullptr };
}; ///< Structure for action mapping.


}


//****************************************************************************************************************************************************
/// \param[in] parent The parent widget of the frame
//****************************************************************************************************************************************************
ComboTableWidget::ComboTableWidget(QWidget *parent)
    : QWidget(parent), ui_(), contextMenu_(nullptr), groupListWidget_(nullptr) {
    ui_.setupUi(this);
    this->setupActions();
    connect(ui_.editSearch, &QLineEdit::textChanged, this, &ComboTableWidget::onSearchFilterChanged);
    this->setupTable();
    this->setupKeyboardShortcuts();
    this->setupCombosMenu();
    this->setupContextMenu();
    this->updateGui();
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void ComboTableWidget::setupActions() {

    QList<::ActionMapping> const actionMappings = {
        { &actionCopySnippet_, tr("&Copy Snippet"), tr("Copy Snippet"), tr("Ctrl+C"), &ComboTableWidget::onActionCopySnippet  },
        { &actionDeleteCombo_, tr("&Delete"), tr("Delete Combo"), tr("Del"), &ComboTableWidget::onActionDeleteCombo  },
        { &actionDeselectAll_, tr("&Deselect All"), tr("Deselect All"), tr("Ctrl+D"), &ComboTableWidget::onActionDeselectAll },
        { &actionDuplicateCombo_, tr("&Duplicate"), tr("Duplicate Combo"), tr("Ctrl+U"), &ComboTableWidget::onActionDuplicateCombo },
        { &actionEditCombo_, tr("&Edit"), tr("Edit Combo"), tr("Return"), &ComboTableWidget::onActionEditCombo },
        { &actionEnableDisableCombo_, tr("&Enable"), tr("Enable Combo"), tr("Ctrl+E"), &ComboTableWidget::onActionEnableDisableCombo },
        { &actionNewCombo_, tr("&New"), tr("New Combo"), tr("Ctrl+N"), &ComboTableWidget::onActionNewCombo },
        { &actionSelectAll_, tr("Select &All"), tr("Select All"), tr("Ctrl+A"), &ComboTableWidget::onActionSelectAll },
		{ &actionExportCombos_, tr("&Export Combos…"), tr("Export Combos"), tr("Ctrl+O"), &ComboTableWidget::onActionExportCombos },
		{ &actionImportCombos_, tr("&Import Combos…"), tr("Import Combos"), tr("Ctrl+I"), &ComboTableWidget::onActionImportCombos },
    };

    for (ActionMapping const& mapping: actionMappings) {
        QAction *action = new QAction(mapping.text, this);
        action->setToolTip(mapping.tooltip);
        action->setShortcut(mapping.shortcut);
        *mapping.action = action;
        connect(action, &QAction::triggered, this, mapping.slot);
    }
}


//****************************************************************************************************************************************************
/// \param[in] groupListWidget The group list widget
//****************************************************************************************************************************************************
void ComboTableWidget::setGroupListWidget(GroupListWidget *groupListWidget) {
    groupListWidget_ = groupListWidget;
}


//****************************************************************************************************************************************************
/// \param[in] filePath The path of the combo file to import. This value can be null
//****************************************************************************************************************************************************
void ComboTableWidget::runComboImportDialog(QString const &filePath) {
    if (QDialog::Accepted == ComboImportDialog(filePath, (groupListWidget_ ? groupListWidget_->selectedGroup() : SpGroup()), this).exec())
        this->updateGui();
}


//****************************************************************************************************************************************************
/// \param[in] parent The parent widget of the menu
//****************************************************************************************************************************************************
QMenu *ComboTableWidget::menu(QWidget *parent) const {
    QMenu *menu = new QMenu(menuTitle(), parent);
    menu->addAction(actionNewCombo_);
    menu->addAction(actionEditCombo_);
    menu->addAction(actionDuplicateCombo_);
    menu->addAction(actionDeleteCombo_);
    QMenu *moveToMenu = ComboManager::instance().groupListRef().createMenu(moveMenuTitle(), std::set<SpGroup>(), menu);
    menu->addMenu(moveToMenu);
    menu->addSeparator();
    menu->addAction(actionCopySnippet_);
    menu->addSeparator();
    menu->addAction(actionEnableDisableCombo_);
    menu->addSeparator();
    menu->addAction(actionSelectAll_);
    menu->addAction(actionDeselectAll_);
    menu->setProperty(kPropMoveToMenu, QVariant::fromValue(moveToMenu));
    connect(menu, &QMenu::aboutToShow, this, &ComboTableWidget::onContextMenuAboutToShow);
    connect(moveToMenu, &QMenu::triggered, this, &ComboTableWidget::onMoveToGroupMenuTriggered);
    connect(moveToMenu, &QMenu::aboutToShow, this, &ComboTableWidget::onMoveToGroupMenuAboutToShow);
    return menu;
}


//****************************************************************************************************************************************************
/// \return The localized title fo the menu
//****************************************************************************************************************************************************
QString ComboTableWidget::menuTitle() {
    return tr("&Combos");
}


//****************************************************************************************************************************************************
/// \param[in] combo The combo to select
//****************************************************************************************************************************************************
void ComboTableWidget::selectCombo(SpCombo const &combo) const {
    if (!combo)
        return;
    // first, we ensure that the current group is <all Combos> or we select the combo's group
    if (groupListWidget_)
        groupListWidget_->selectGroup(combo->group());
    ComboList &comboList = ComboManager::instance().comboListRef();
    for (qint32 i = 0; i < comboList.size(); ++i) {
        SpCombo const &c = comboList[i];
        if (c && (c->uuid() == combo->uuid())) {
            ui_.tableComboList->selectRow(proxyModel_.mapFromSource(comboList.index(i, 0)).row());
            return;
        }
    }
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onSelectedGroupChanged(SpGroup const &group) {
    proxyModel_.setGroup(group);
    this->resizeColumnsToContents();
    QItemSelectionModel *model = ui_.tableComboList->selectionModel();
    if (model)
        model->clear();
    this->updateGui();
}


//****************************************************************************************************************************************************
/// This event filter override the default mouse double click behavior of the table view to respond also 
/// when the user double click on an empty area
//****************************************************************************************************************************************************
bool ComboTableWidget::eventFilter(QObject *object, QEvent *event) {
    if (event->type() != QEvent::MouseButtonDblClick)
        return QObject::eventFilter(object, event);
    this->onDoubleClick();
    return true;
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::setupTable() {
    proxyModel_.setSourceModel(&ComboManager::instance().comboListRef());
    ui_.tableComboList->setModel(&proxyModel_);
    proxyModel_.sort(0, Qt::AscendingOrder);
    QHeaderView *horizontalHeader = ui_.tableComboList->horizontalHeader();
    if (!horizontalHeader)
        throw xmilib::Exception(QString("Internal error: %1(): could not retrieve horizontal header.")
            .arg(__FUNCTION__));
    horizontalHeader->setFixedHeight(horizontalHeader->fontMetrics().height() + 10);
    horizontalHeader->setSortIndicator(0, Qt::AscendingOrder);
    // required, otherwise the indicator is first displayed in the wrong direction
    horizontalHeader->setDefaultAlignment(Qt::AlignLeft);
    connect(ui_.tableComboList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ComboTableWidget::updateGui);
    connect(&ComboManager::instance().groupListRef(), &GroupList::combosChangedGroup, this, &ComboTableWidget::onComboChangedGroup);
    QHeaderView *verticalHeader = ui_.tableComboList->verticalHeader();

    if (!verticalHeader)
        throw xmilib::Exception(QString("Internal error: %1(): could not retrieve vertical header.")
            .arg(__FUNCTION__));
    verticalHeader->setDefaultSectionSize(verticalHeader->fontMetrics().height() + 10);

    this->resizeColumnsToContents();
    ui_.tableComboList->viewport()->installEventFilter(this);
    // we install an event filter that override the default double-click behavior
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::setupKeyboardShortcuts() {
    // Most shortcuts are set via the actions in the widget UI form. However, shortcuts work only if the action are
    // used. Although actions are not used in the form itself, they are used in the menu, instead for the ones that are
    // defined below

    connect(new QShortcut(QKeySequence("Ctrl+F"), this), &QShortcut::activated, this, &ComboTableWidget::onActionStartSearch);
    connect(new QShortcut(QKeySequence("Ctrl+Shift+F"), this), &QShortcut::activated, this, &ComboTableWidget::onActionStartSearchInAllGroups);
    connect(new QShortcut(QKeySequence("Escape"), this), &QShortcut::activated, this, &ComboTableWidget::onActionClearSearch);
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::setupCombosMenu() {
    ui_.buttonCombos->setMenu(this->menu(this));
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::setupContextMenu() {
    if (contextMenu_)
        contextMenu_->deleteLater();
    contextMenu_ = this->menu(this);
    ui_.tableComboList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui_.tableComboList, &QTableView::customContextMenuRequested, this, &ComboTableWidget::onContextMenuRequested);
}


//****************************************************************************************************************************************************
/// \return The number of selected combos
//****************************************************************************************************************************************************
qint32 ComboTableWidget::selectedComboCount() const {
    return qint32(ui_.tableComboList->selectionModel()->selectedRows().size());
}


//****************************************************************************************************************************************************
/// \return The first selected combo
/// \return a null combo if none is selected
//****************************************************************************************************************************************************
SpCombo ComboTableWidget::getSelectedCombo() const {
    QList<qint32> const selectedIndexes = this->getSelectedComboIndexes();
    if (selectedIndexes.isEmpty())
        return SpCombo();
    ComboList &comboList = ComboManager::instance().comboListRef();
    qint32 const index = selectedIndexes[0];
    return (index >= 0) && (index < comboList.size()) ? comboList[index] : nullptr;
}


//****************************************************************************************************************************************************
/// The returned indexes are based on the internal order of the list, not the display order in the table (that can be
/// modified by sorting by columns).
///
/// \return The list of indexes of the selected combos 
//****************************************************************************************************************************************************
QList<qint32> ComboTableWidget::getSelectedComboIndexes() const {
    QList<qint32> result;
    ComboList const &comboList = ComboManager::instance().comboListRef();
    QModelIndexList selectedRows = ui_.tableComboList->selectionModel()->selectedRows();
    for (QModelIndex const &modelIndex: selectedRows) {
        qint32 const index = proxyModel_.mapToSource(modelIndex).row();
        if ((index >= 0) && (index < comboList.size()))
            result.push_back(index);
    }
    return result;
}


//****************************************************************************************************************************************************
/// \return The list of selected combos
//****************************************************************************************************************************************************
QList<SpCombo> ComboTableWidget::getSelectedCombos() const {
    QList<SpCombo> result;
    ComboList &comboList = ComboManager::instance().comboListRef();
    QModelIndexList selectedRows = ui_.tableComboList->selectionModel()->selectedRows();
    for (QModelIndex const &modelIndex: selectedRows) {
        qint32 const index = proxyModel_.mapToSource(modelIndex).row();
        if ((index >= 0) && (index < comboList.size()))
            result.push_back(comboList[index]);
    }
    return result;
}


//****************************************************************************************************************************************************
/// \param[in] event The event
//****************************************************************************************************************************************************
void ComboTableWidget::changeEvent(QEvent *event) {
    if (QEvent::LanguageChange == event->type())
        ui_.retranslateUi(this);
    QWidget::changeEvent(event);
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::resizeColumnsToContents() const {
    QHeaderView *horizontalHeader = ui_.tableComboList->horizontalHeader();
    if (!horizontalHeader)
        throw xmilib::Exception(QString("Internal error: %1(): could not retrieve horizontal header").arg(__FUNCTION__));

    for (qint32 i = 0; i < ComboManager::instance().comboListRef().columnCount(QModelIndex()); ++i)
        ui_.tableComboList->resizeColumnToContents(i);
    horizontalHeader->resizeSection(0, qBound<qint32>(kMinNameColumnDefaultWidth, horizontalHeader->sectionSize(0), kMaxNameColumnDefaultWidth));
    horizontalHeader->resizeSection(2, qMin<qint32>(horizontalHeader->sectionSize(2), kMaxSnippetColumnWDefaultWith));
}


//****************************************************************************************************************************************************
/// \return A set containing the groups of the selected combos
//****************************************************************************************************************************************************
std::set<SpGroup> ComboTableWidget::groupsOfSelectedCombos() const {
    std::set<SpGroup> groups;
    QList<SpCombo> combos = this->getSelectedCombos();
    for (SpCombo const &combo: combos) {
        if (!combo)
            continue;
        SpGroup const group = combo->group();
        if (group)
            groups.insert(group);
    }
    return groups;
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::updateGui() const {
    qint32 const selectedCount = this->selectedComboCount();
    bool const listIsEmpty = (ComboManager::instance().comboListRef().size() == 0);
    bool const hasOneSelected = (1 == selectedCount);
    bool const hasOneOrMoreSelected = (selectedCount > 0);
    actionDuplicateCombo_->setEnabled(hasOneSelected);
    actionDeleteCombo_->setEnabled(hasOneOrMoreSelected);
    actionEditCombo_->setEnabled(hasOneSelected);
    actionCopySnippet_->setEnabled(hasOneSelected);
    actionEnableDisableCombo_->setEnabled(hasOneSelected);
	actionExportCombos_->setEnabled(!listIsEmpty);
    QString enableDisableText = tr("Ena&ble");
    QString enableDisableToolTip = tr("Enable combo");
    QString enableDisableIconText = tr("Enable the combo");
    if ((hasOneSelected)
        && (ComboManager::instance().comboListRef()[this->getSelectedComboIndexes().front()]->isEnabled())) {
        enableDisableText = tr("Disa&ble");
        enableDisableToolTip = tr("Disable combo");
        enableDisableIconText = tr("Disable the combo");
    }
    actionEnableDisableCombo_->setText(enableDisableText);
    actionEnableDisableCombo_->setToolTip(enableDisableToolTip);
    actionEnableDisableCombo_->setIconText(enableDisableIconText);
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onActionStartSearch() const {
    this->ui_.editSearch->setFocus();
    this->ui_.editSearch->selectAll();
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onActionStartSearchInAllGroups() const {
    if (groupListWidget_)
        groupListWidget_->selectAllCombosEntry();
    this->onActionStartSearch();
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onActionClearSearch() const {
    this->ui_.editSearch->setFocus();
    this->ui_.editSearch->clear();
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onActionNewCombo() {
    try {
        SpCombo const combo = Combo::create();
        combo->setGroup(groupListWidget_->selectedOrFirstGroup());

        if (!ComboDialog::run(combo, tr("New Combo")))
            return;
        ComboManager const &comboManager = ComboManager::instance();
        ComboList &comboList = ComboManager::instance().comboListRef();
        if (!comboList.append(combo))
            throw xmilib::Exception(tr("The combo could not be added to the list."));
        QString errorMessage;
        if (!comboManager.saveComboListToFile(&errorMessage))
            throw xmilib::Exception(errorMessage);
        this->selectCombo(combo);
        this->updateGui();
    }
    catch (xmilib::Exception const &e) {
        QMessageBox::critical(this, tr("Error"), e.qwhat());
    }
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onActionDuplicateCombo() {
    try {
        QList<qint32> const selectedIndex = this->getSelectedComboIndexes();
        if (1 != selectedIndex.size())
            return;
        ComboManager &comboManager = ComboManager::instance();
        ComboList &comboList = comboManager.comboListRef();
        qint32 const index = selectedIndex[0];
        Q_ASSERT((index >= 0) && (index < comboList.size()));
        if ((index < 0) || (index >= comboList.size()))
            throw xmilib::Exception(tr("The combo could not be duplicated: invalid index."));
        SpCombo const combo = Combo::duplicate(*comboList[index]);
        if (!ComboDialog::run(combo, tr("Duplicate Combo")))
            return;
        if (!comboList.append(combo))
            throw xmilib::Exception(tr("The duplicated combo could not added to the list."));
        QString errorMessage;
        if (!comboManager.saveComboListToFile(&errorMessage))
            throw xmilib::Exception(errorMessage);
        this->selectCombo(combo);
        this->updateGui();
    }
    catch (xmilib::Exception const &e) {
        QMessageBox::critical(this, tr("Error"), e.qwhat());
    }
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onActionDeleteCombo() {
    qint32 const count = this->selectedComboCount();
    if (count < 1)
        return;
    QString const question = count > 1 ? tr("Are you sure you want to delete the selected combos?")
        : tr("Are you sure you want to delete the selected combo?");
    if (QMessageBox::Yes != QMessageBox::question(nullptr, tr("Delete Combo?"), question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No))
        return;

    ComboManager &comboManager = ComboManager::instance();
    QList<qint32> indexes = this->getSelectedComboIndexes();
    std::sort(indexes.begin(), indexes.end(), [](qint32 first, qint32 second) -> bool { return first > second; });
    for (qint32 const index: indexes)
        comboManager.comboListRef().erase(index);
    QString errorMessage;
    if (!comboManager.saveComboListToFile(&errorMessage))
        QMessageBox::critical(this, tr("Error"), errorMessage);
    this->updateGui();
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onActionEditCombo() {
    QList<qint32> const selectedIndex = this->getSelectedComboIndexes();
    if (1 != selectedIndex.size())
        return;

    ComboManager &comboManager = ComboManager::instance();
    ComboList &comboList = comboManager.comboListRef();
    qint32 const index = selectedIndex[0];
    Q_ASSERT((index >= 0) && (index < comboList.size()));
    SpCombo const combo = comboList[index];
    if (!ComboDialog::run(combo, tr("Edit Combo")))
        return;
    comboList.markComboAsEdited(index);
    QString errorMessage;
    if (!comboManager.saveComboListToFile(&errorMessage))
        QMessageBox::critical(this, tr("Error"), errorMessage);
    proxyModel_.invalidate();
    this->selectCombo(combo);
    this->updateGui();
    this->resizeColumnsToContents();
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onActionCopySnippet() {
    SpCombo const combo = this->getSelectedCombo();
    if (!combo)
        return;
    bool cancelled = false;
    QString text = combo->evaluatedSnippet(cancelled);
    if (cancelled)
        return;
    if constexpr (constants::kRestrictedBuild)
        text = tlf::prepareSnippet(text,
            PreferencesManager::instance().allowRealLineBreaksInSnippets()).text;
    else
        text.remove(constants::kVariableRegExp); ///< Remove all remaining variables (#{cursor},  #{delay:},... )
    QGuiApplication::clipboard()->setText(text);
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onActionSelectAll() {
    ui_.tableComboList->selectAll();
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onActionDeselectAll() {
    ui_.tableComboList->clearSelection();
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onActionEnableDisableCombo() {
    QList<qint32> const selectedIndex = this->getSelectedComboIndexes();
    if (1 != selectedIndex.size())
        return;

    ComboManager &comboManager = ComboManager::instance();
    ComboList &comboList = comboManager.comboListRef();
    qint32 const index = selectedIndex[0];
    Q_ASSERT((index >= 0) && (index < comboList.size()));
    SpCombo const combo = comboList[index];
    combo->setEnabled(!combo->isEnabled());
    comboList.markComboAsEdited(index);
    QString errorMessage;
    if (!comboManager.saveComboListToFile(&errorMessage))
        QMessageBox::critical(this, tr("Error"), errorMessage);
    this->updateGui();
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onActionExportCombos() {
	ComboList const &comboList = ComboManager::instance().comboListRef();
	if (comboList.isEmpty())
		return;

	QList<qint32> const selectedIndexes = this->getSelectedComboIndexes();
	ComboExportDialog scopeDialog(selectedIndexes.size(), comboList.size(), this);
	if (scopeDialog.exec() != QDialog::Accepted)
		return;

	QJsonArray combos;
	if (scopeDialog.scope() == ComboExportDialog::EScope::Selected) {
		for (qint32 const index: selectedIndexes) {
			Q_ASSERT(index >= 0 && index < comboList.size());
			combos.append(comboList[index]->toJsonObject(true));
		}
	} else {
		for (SpCombo const &combo: comboList)
			combos.append(combo->toJsonObject(true));
	}

	QJsonArray const allGroups = ComboManager::instance().groupListRef().toJsonArray();
	QJsonArray const groups = scopeDialog.scope() == ComboExportDialog::EScope::Selected
		? combo_portability::referencedGroups(combos, allGroups) : allGroups;

	PreferencesManager const &prefs = PreferencesManager::instance();
	QString folder = prefs.lastComboImportExportPath();
	if (!folder.isEmpty() && !QFileInfo(folder).isDir())
		folder = QFileInfo(folder).absolutePath();
	if (folder.isEmpty() || !QFileInfo(folder).isDir())
		folder = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

	QFileDialog fileDialog(this, tr("Export Combos"),
		QDir(folder).absoluteFilePath(combo_portability::defaultExportFileName()),
		combo_portability::exportFileDialogFilter());
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);
	fileDialog.setFileMode(QFileDialog::AnyFile);
	fileDialog.setDefaultSuffix("txt");
	if (fileDialog.exec() != QDialog::Accepted || fileDialog.selectedFiles().isEmpty())
		return;
	QString path = fileDialog.selectedFiles().constFirst();
	if (QFileInfo(path).suffix().compare("txt", Qt::CaseInsensitive) != 0)
		path = QDir(QFileInfo(path).absolutePath()).absoluteFilePath(QFileInfo(path).completeBaseName() + ".txt");
	prefs.setLastComboImportExportPath(path);

	QString errorMessage;
	if (!combo_portability::saveLeanBundle(path, combos, groups, &errorMessage)) {
		globals::debugLog().addError(errorMessage);
		QMessageBox::critical(this, tr("Error"), errorMessage);
	}
}


//****************************************************************************************************************************************************
/// \param[in] parent The parent widget of the menu.
/// \return The top-level Combos menu containing the complete user-facing portability model.
//****************************************************************************************************************************************************
QMenu *ComboTableWidget::portabilityMenu(QWidget *parent) const {
	QMenu *menu = new QMenu(menuTitle(), parent);
	menu->addAction(actionImportCombos_);
	menu->addAction(actionExportCombos_);
	return menu;
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onActionImportCombos() {
    this->runComboImportDialog();
    this->resizeColumnsToContents();
}


//****************************************************************************************************************************************************
/// \param[in] text The text to search
//****************************************************************************************************************************************************
void ComboTableWidget::onSearchFilterChanged(QString const &text) {
    this->style()->polish(ui_.editSearch);
    proxyModel_.setFilterFixedString(text.trimmed());
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onContextMenuRequested() const {
    contextMenu_->exec(QCursor::pos());
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onDoubleClick() {
    switch (this->selectedComboCount()) {
    case 0:
        this->onActionNewCombo();
        break;
    case 1:
        this->onActionEditCombo();
        break;
    default:
        break;
    }
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onComboChangedGroup() {
    proxyModel_.invalidate();
    if (!ComboManager::instance().saveComboListToFile())
        throw xmilib::Exception("Could not save combo list.");
    this->resizeColumnsToContents();
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void ComboTableWidget::onContextMenuAboutToShow() const {
    QMenu const *menu = dynamic_cast<QMenu *>(this->sender());
    if (!menu)
        throw xmilib::Exception(QString("Internal error: %1(): could not retrieve context menu.").arg(__FUNCTION__));
    QMenu *moveToMenu = qvariant_cast<QMenu *>(menu->property(kPropMoveToMenu));
    if (!moveToMenu)
        throw xmilib::Exception(QString("Internal error: %1(): could not retrieve 'Move To' menu.").arg(__FUNCTION__));
    bool const hasOneOrMoreSelected = this->selectedComboCount() > 0;
    moveToMenu->setEnabled(hasOneOrMoreSelected);
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void ComboTableWidget::onMoveToGroupMenuAboutToShow() const {
    QMenu *menu = dynamic_cast<QMenu *>(this->sender());
    if (!menu)
        throw xmilib::Exception(QString("Internal error: %1(): could not retrieve context menu.").arg(__FUNCTION__));

    std::set<SpGroup> notInGroup = this->groupsOfSelectedCombos();
    if (notInGroup.size() > 1) ///< If the selected combos belong to more than one group, we do not disabled any
        notInGroup.clear();
    ComboManager::instance().groupListRef().fillMenu(menu, notInGroup);
    menu->setEnabled(this->selectedComboCount());
}


//****************************************************************************************************************************************************
/// \param[in] action The action
//****************************************************************************************************************************************************
void ComboTableWidget::onMoveToGroupMenuTriggered(QAction const *action) {
    if (!action)
        return;
    SpGroup const group = action->data().value<SpGroup>();
    if (!group)
        throw xmilib::Exception(QString("Internal error: %1(): could not retrieve group.").arg(__FUNCTION__));
    QList<SpCombo> const combos = this->getSelectedCombos();
    for (SpCombo const &combo: combos)
        if (combo && (group != combo->group()))
            combo->setGroup(group);
    this->onComboChangedGroup();
}
