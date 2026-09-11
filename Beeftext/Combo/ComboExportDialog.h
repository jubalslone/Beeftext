/// \file
///
/// \brief Declaration of the Lean combo export scope dialog.


#ifndef BEEFTEXT_COMBO_EXPORT_DIALOG_H
#define BEEFTEXT_COMBO_EXPORT_DIALOG_H


#include <QDialog>


class QRadioButton;


class ComboExportDialog : public QDialog {
Q_OBJECT
public:
	enum class EScope {
		Selected,
		All,
	};

	explicit ComboExportDialog(qint32 selectedCount, QWidget *parent = nullptr);
	EScope scope() const;

private:
	QRadioButton *selectedRadio_ { nullptr };
};


#endif // BEEFTEXT_COMBO_EXPORT_DIALOG_H
