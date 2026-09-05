/// \file
///
/// \brief Implementation of the Lean combo export scope dialog.


#include "stdafx.h"
#include "ComboExportDialog.h"

#include <XMiLib/XMiLibConstants.h>


ComboExportDialog::ComboExportDialog(qint32 selectedCount, QWidget *parent)
	: QDialog(parent, xmilib::constants::kDefaultDialogFlags) {
	setWindowTitle(tr("Export Combos"));
	QVBoxLayout *layout = new QVBoxLayout(this);
	QGroupBox *scopeGroup = new QGroupBox(tr("Choose what to export"), this);
	QVBoxLayout *scopeLayout = new QVBoxLayout(scopeGroup);
	selectedRadio_ = new QRadioButton(tr("Selected combos (%1)").arg(selectedCount), scopeGroup);
	selectedRadio_->setEnabled(selectedCount > 0);
	QRadioButton *allRadio = new QRadioButton(tr("All combos"), scopeGroup);
	scopeLayout->addWidget(selectedRadio_);
	scopeLayout->addWidget(allRadio);
	layout->addWidget(scopeGroup);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
	buttons->button(QDialogButtonBox::Save)->setText(tr("Export"));
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	layout->addWidget(buttons);

	(selectedCount > 0 ? selectedRadio_ : allRadio)->setChecked(true);
}


ComboExportDialog::EScope ComboExportDialog::scope() const {
	return selectedRadio_->isChecked() ? EScope::Selected : EScope::All;
}
