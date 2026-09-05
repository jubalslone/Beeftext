/// \file
/// \author Xavier Michelon
///
/// \brief Implementation of preferences dialog
///
/// Copyright (c) Xavier Michelon. All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.  


#include "stdafx.h"
#include "PreferencesDialog.h"
#include <XMiLib/XMiLibConstants.h>
#include <QAbstractSpinBox>
#include <QBoxLayout>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QStyleOption>


namespace {


struct ComfortableLayoutMetrics {
    int horizontalSpacing;
    int verticalSpacing;
    int outerLeftMargin;
    int outerTopMargin;
    int outerRightMargin;
    int outerBottomMargin;
    int groupHorizontalMargin;
    int groupTopMargin;
    int groupBottomMargin;
};


int stylePixelMetric(QWidget const &widget, QStyle::PixelMetric metric, int fallback) {
    int const value = widget.style()->pixelMetric(metric, nullptr, &widget);
    return value >= 0 ? value : fallback;
}


ComfortableLayoutMetrics comfortableLayoutMetrics(QWidget const &widget) {
    int const fontHeight = widget.fontMetrics().height();
    int const halfLine = qMax(1, (fontHeight + 1) / 2);
    int const threeQuarterLine = qMax(1, (fontHeight * 3 + 3) / 4);

    ComfortableLayoutMetrics metrics;
    metrics.horizontalSpacing = qMax(stylePixelMetric(widget, QStyle::PM_LayoutHorizontalSpacing, 0), threeQuarterLine);
    metrics.verticalSpacing = qMax(stylePixelMetric(widget, QStyle::PM_LayoutVerticalSpacing, 0), halfLine);
    metrics.outerLeftMargin = qMax(stylePixelMetric(widget, QStyle::PM_LayoutLeftMargin, 0), threeQuarterLine);
    metrics.outerTopMargin = qMax(stylePixelMetric(widget, QStyle::PM_LayoutTopMargin, 0), threeQuarterLine);
    metrics.outerRightMargin = qMax(stylePixelMetric(widget, QStyle::PM_LayoutRightMargin, 0), threeQuarterLine);
    metrics.outerBottomMargin = qMax(stylePixelMetric(widget, QStyle::PM_LayoutBottomMargin, 0), threeQuarterLine);
    metrics.groupHorizontalMargin = qMax(metrics.outerLeftMargin, threeQuarterLine);
    metrics.groupTopMargin = qMax(metrics.outerTopMargin, fontHeight);
    metrics.groupBottomMargin = qMax(metrics.outerBottomMargin, halfLine);
    return metrics;
}


void setLayoutSpacing(QLayout &layout, ComfortableLayoutMetrics const &metrics) {
    if (auto *gridLayout = qobject_cast<QGridLayout *>(&layout)) {
        gridLayout->setHorizontalSpacing(metrics.horizontalSpacing);
        gridLayout->setVerticalSpacing(metrics.verticalSpacing);
    }
    else if (auto *boxLayout = qobject_cast<QBoxLayout *>(&layout)) {
        bool const horizontal = (boxLayout->direction() == QBoxLayout::LeftToRight)
                                || (boxLayout->direction() == QBoxLayout::RightToLeft);
        boxLayout->setSpacing(horizontal ? metrics.horizontalSpacing : metrics.verticalSpacing);
    }
}


void applyComfortablePaneLayout(QWidget &pane, ComfortableLayoutMetrics const &metrics) {
    if (QLayout *paneLayout = pane.layout()) {
        paneLayout->setContentsMargins(metrics.outerLeftMargin, metrics.outerTopMargin,
                                      metrics.outerRightMargin, metrics.outerBottomMargin);
    }

    for (QLayout *layout: pane.findChildren<QLayout *>())
        setLayoutSpacing(*layout, metrics);

    for (QGroupBox *groupBox: pane.findChildren<QGroupBox *>()) {
        if (QLayout *groupLayout = groupBox->layout()) {
            groupLayout->setContentsMargins(metrics.groupHorizontalMargin, metrics.groupTopMargin,
                                            metrics.groupHorizontalMargin, metrics.groupBottomMargin);
        }
    }
}


template<typename StyleOption>
void ensureFontAwareHeight(QWidget &widget, QStyle::ContentsType contentsType) {
    StyleOption option;
    option.initFrom(&widget);
    QSize const fontContentSize(0, widget.fontMetrics().height());
    int const styleHeight = widget.style()->sizeFromContents(contentsType, &option, fontContentSize, &widget).height();
    widget.setMinimumHeight(qMax(styleHeight, qMax(widget.minimumSizeHint().height(), widget.sizeHint().height())));
}


void ensureFontAwareControlHeights(QWidget &root) {
    for (QComboBox *comboBox: root.findChildren<QComboBox *>())
        ensureFontAwareHeight<QStyleOptionComboBox>(*comboBox, QStyle::CT_ComboBox);

    for (QAbstractSpinBox *spinBox: root.findChildren<QAbstractSpinBox *>())
        ensureFontAwareHeight<QStyleOptionSpinBox>(*spinBox, QStyle::CT_SpinBox);

    for (QLineEdit *lineEdit: root.findChildren<QLineEdit *>()) {
        if (!qobject_cast<QAbstractSpinBox *>(lineEdit->parentWidget()))
            ensureFontAwareHeight<QStyleOptionFrame>(*lineEdit, QStyle::CT_LineEdit);
    }

    for (QPushButton *button: root.findChildren<QPushButton *>())
        ensureFontAwareHeight<QStyleOptionButton>(*button, QStyle::CT_PushButton);
}


}


//****************************************************************************************************************************************************
/// \param[in] parent The parent widget of the dialog
//****************************************************************************************************************************************************
PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent, xmilib::constants::kDefaultDialogFlags), prefs_(PreferencesManager::instance()) {
    ui_.setupUi(this);

    connect(ui_.buttonResetWarnings, &QPushButton::clicked, this, &PreferencesDialog::onResetWarnings);
    connect(ui_.buttonDefaults, &QPushButton::clicked, this, &PreferencesDialog::onResetToDefaultValues);
    connect(ui_.buttonClose, &QPushButton::clicked, this, &PreferencesDialog::onClose);

    panes_ = { ui_.paneBehavior, ui_.paneCombos, ui_.paneEmojis, ui_.paneAppearance, ui_.paneAdvanced };
    baseMinimumSize_ = this->minimumSize();
    this->load();
    ensureFontAwareControlHeights(*this);
    this->updateComfortableLayout(true);
    ui_.tabPreferences->setCurrentIndex(0);
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void PreferencesDialog::load() const {
    for (PrefPane const *pane: panes_)
        pane->load();
}


//****************************************************************************************************************************************************
/// \return true if and only if the settings are consistent.
//****************************************************************************************************************************************************
bool PreferencesDialog::validateInput() {
    for (PrefPane *pane: panes_)
        if (!pane->validateInput())
            return false;
    return true;
}


//****************************************************************************************************************************************************
/// \param[in] event The event.
//****************************************************************************************************************************************************
void PreferencesDialog::changeEvent(QEvent *event) {
    if (QEvent::LanguageChange == event->type())
        ui_.retranslateUi(this);
    QDialog::changeEvent(event);

    if ((QEvent::LanguageChange == event->type()) || (QEvent::FontChange == event->type())
        || (QEvent::ApplicationFontChange == event->type()) || (QEvent::StyleChange == event->type())) {
        ensureFontAwareControlHeights(*this);
        this->updateComfortableLayout(false);
    }
}


//****************************************************************************************************************************************************
/// \param[in] resizeToMinimum Whether to reset the dialog to the newly calculated comfortable size.
//****************************************************************************************************************************************************
void PreferencesDialog::updateComfortableLayout(bool resizeToMinimum) {
    ComfortableLayoutMetrics const metrics = comfortableLayoutMetrics(*this);
    ui_.verticalLayout->setContentsMargins(metrics.outerLeftMargin, metrics.outerTopMargin,
                                          metrics.outerRightMargin, metrics.outerBottomMargin);
    ui_.verticalLayout->setSpacing(metrics.verticalSpacing);
    ui_.horizontalLayout->setSpacing(metrics.horizontalSpacing);

    for (PrefPane *pane: panes_)
        applyComfortablePaneLayout(*pane, metrics);

    // Reset before asking the layout for its size hint so repeated font/style changes
    // cannot compound the previous minimum size.
    this->setMinimumSize(baseMinimumSize_);
    ui_.verticalLayout->invalidate();
    ui_.verticalLayout->activate();

    QSize comfortableSize = ui_.verticalLayout->sizeHint();
    comfortableSize.rwidth() += metrics.outerLeftMargin + metrics.outerRightMargin;
    comfortableSize.rheight() += 2 * metrics.verticalSpacing;
    comfortableSize = comfortableSize.expandedTo(baseMinimumSize_);
    this->setMinimumSize(comfortableSize);
    this->resize(resizeToMinimum ? comfortableSize : this->size().expandedTo(comfortableSize));
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void PreferencesDialog::onResetToDefaultValues() {
    if (QMessageBox::Yes != QMessageBox::question(this, tr("Reset Preferences"), tr("Are you sure you want to reset "
                                                                                    "the preferences to their default values?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No))
        return;

    prefs_.reset();
    this->load();
}


//****************************************************************************************************************************************************
//
//****************************************************************************************************************************************************
void PreferencesDialog::onResetWarnings() {
    if (QMessageBox::Yes == QMessageBox::question(this, tr("Reset Warnings"), tr("Are you sure you want to reset "
                                                                                 "all warnings?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No))
        prefs_.resetWarnings();
}


//****************************************************************************************************************************************************
// 
//****************************************************************************************************************************************************
void PreferencesDialog::onClose() {
    if (this->validateInput())
        this->close();
}
