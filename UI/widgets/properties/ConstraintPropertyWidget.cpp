#include "ConstraintPropertyWidget.h"

#include "../../../core/commands/state/UpdateConstraintConditionCommand.h"
#include "../../../utils/ColorManager.h"
#include "../../../utils/StyleManager.h"
#include "../../visualization/VisualizationManager.h"
#include "PlacementEditor.h"
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QIntValidator>

ConstraintPropertyWidget::ConstraintPropertyWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void ConstraintPropertyWidget::setUIState(UIState* uiState)
{
    if (m_uiState) {
        disconnect(m_uiState, &UIState::boundaryConditionChanged, this, nullptr);
    }
    m_uiState = uiState;

    // Refresh when the constraint changes elsewhere (e.g. double-clicking a face in the viewer)
    if (m_uiState) {
        connect(m_uiState, &UIState::boundaryConditionChanged, this, [this]() { updateData(); });
    }
}

void ConstraintPropertyWidget::setVisualizationManager(VisualizationManager* vizManager)
{
    if (m_vizManager) {
        disconnect(m_vizManager, &VisualizationManager::edgeClicked, this, nullptr);
    }
    m_vizManager = vizManager;
    if (m_vizManager) {
        connect(m_vizManager, &VisualizationManager::edgeClicked, this, &ConstraintPropertyWidget::onEdgeSelected);
    }
}

void ConstraintPropertyWidget::setTarget(int index)
{
    m_currentIndex = index;
    updateData();
}

void ConstraintPropertyWidget::setupUI()
{
    QFormLayout* layout = new QFormLayout(this);
    layout->setLabelAlignment(Qt::AlignLeft);
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    layout->setSpacing(StyleManager::FORM_SPACING);
    layout->setContentsMargins(StyleManager::FORM_SPACING, StyleManager::FORM_SPACING,
                               StyleManager::FORM_SPACING, StyleManager::FORM_SPACING);

    // Read-only hint label (initially hidden)
    m_readOnlyHintLabel = new QLabel("Go back to Step 2 to edit.");
    m_readOnlyHintLabel->setStyleSheet(QString("color: %1; font-size: %2px; padding: 2px 4px;")
        .arg(ColorManager::ACCENT_COLOR.name())
        .arg(StyleManager::FONT_SIZE_SMALL));
    m_readOnlyHintLabel->setVisible(false);
    layout->addRow(m_readOnlyHintLabel);

    // Labels style
    QString labelStyle = "color: #aaaaaa;";

    // Updated input style: unified width, rounded corners, min-height to prevent collapse
    QString inputStyle = QString("QLineEdit { color: %1; background-color: %2; border: 1px solid %3; padding: %4px; border-radius: %5px; min-height: %6px; selection-background-color: #555555; }")
        .arg(ColorManager::INPUT_TEXT_COLOR.name())
        .arg(ColorManager::INPUT_BACKGROUND_COLOR.name())
        .arg(ColorManager::INPUT_BORDER_COLOR.name())
        .arg(StyleManager::PADDING_SMALL)
        .arg(StyleManager::RADIUS_SMALL)
        .arg(StyleManager::INPUT_HEIGHT_SMALL);
    
    // Name
    m_nameEdit = new QLineEdit();
    m_nameEdit->setStyleSheet(inputStyle);
    m_nameEdit->setFixedWidth(100);
    connect(m_nameEdit, &QLineEdit::editingFinished, this, &ConstraintPropertyWidget::pushData);
    layout->addRow(new QLabel("Name:"), m_nameEdit);
    
    // Surface ID
    m_surfaceIdEdit = new QLineEdit();
    m_surfaceIdEdit->setValidator(new QIntValidator(0, 99999, this));
    m_surfaceIdEdit->setStyleSheet(inputStyle);
    m_surfaceIdEdit->setFixedWidth(100);
    // Connect to editingFinished for data push (consistent with LoadPropertyWidget)
    connect(m_surfaceIdEdit, &QLineEdit::editingFinished, this, &ConstraintPropertyWidget::pushData);
    layout->addRow(new QLabel("Surface ID:"), m_surfaceIdEdit);

    // Where the constraint applies: whole face, patch at a point, or an edge
    PlacementEditor::Options options;
    options.direction = false;
    options.edge = true;
    m_placementEditor = new PlacementEditor(this, layout, 100, options);
    connect(m_placementEditor, &PlacementEditor::changed, this, &ConstraintPropertyWidget::pushData);
    connect(m_placementEditor, &PlacementEditor::pointModeEnabled, this, &ConstraintPropertyWidget::onPointModeEnabled);
    connect(m_placementEditor, &PlacementEditor::edgeSelectionRequested, this, [this]() {
        setEdgeSelecting(!m_isSelectingEdge);
    });
    
    // Apply label style
    for(int i = 0; i < layout->rowCount(); ++i) {
        QLayoutItem* item = layout->itemAt(i, QFormLayout::LabelRole);
        if(item && item->widget()) item->widget()->setStyleSheet(labelStyle);
    }

    // Spacer to push Close button to the bottom
    layout->addItem(new QSpacerItem(0, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));

    // Close Button container for right alignment
    QWidget* closeButtonContainer = new QWidget();
    QHBoxLayout* closeButtonLayout = new QHBoxLayout(closeButtonContainer);
    closeButtonLayout->setContentsMargins(0, 0, 0, 0);
    closeButtonLayout->addStretch();

    m_closeButton = new QPushButton("Close");
    m_closeButton->setFixedWidth(80);
    m_closeButton->setStyleSheet(
        QString("QPushButton { background-color: %1; color: %2; border: none; "
                "padding: %5px %6px; border-radius: %7px; font-weight: bold; }"
                "QPushButton:hover { background-color: %3; }"
                "QPushButton:pressed { background-color: %4; }")
        .arg(ColorManager::ACCENT_COLOR.name())
        .arg(ColorManager::BUTTON_TEXT_COLOR.name())
        .arg(ColorManager::BUTTON_HOVER_COLOR.name())
        .arg(ColorManager::BUTTON_PRESSED_COLOR.name())
        .arg(StyleManager::BUTTON_PADDING_V)
        .arg(StyleManager::BUTTON_PADDING_H)
        .arg(StyleManager::BUTTON_RADIUS)
    );
    connect(m_closeButton, &QPushButton::clicked, this, &ConstraintPropertyWidget::onCloseClicked);
    closeButtonLayout->addWidget(m_closeButton);

    layout->addRow("", closeButtonContainer);
}

void ConstraintPropertyWidget::updateData()
{
    if (!m_uiState || m_currentIndex < 0) return;
    
    auto bc = m_uiState->getBoundaryCondition();
    if (m_currentIndex >= (int)bc.constraints.size()) return;
    
    const auto& c = bc.constraints[m_currentIndex];
    
    bool oldBlockedName = m_nameEdit->blockSignals(true);
    m_nameEdit->setText(QString::fromStdString(c.name));
    m_nameEdit->blockSignals(oldBlockedName);
    
    bool oldBlockedId = m_surfaceIdEdit->blockSignals(true);
    m_surfaceIdEdit->setText(QString::number(c.surface_id));
    m_surfaceIdEdit->blockSignals(oldBlockedId);

    m_placementEditor->setConstraint(c);
}

void ConstraintPropertyWidget::pushData()
{
    if (!m_uiState || m_currentIndex < 0) return;
    
    auto bc = m_uiState->getBoundaryCondition();
    if (m_currentIndex >= (int)bc.constraints.size()) return;
    
    ConstraintCondition c = bc.constraints[m_currentIndex];
    c.name = m_nameEdit->text().toStdString();
    c.surface_id = m_surfaceIdEdit->text().toInt();
    m_placementEditor->applyTo(c);
    if (c.target == ConstraintTarget::Edge && m_vizManager) {
        c.edge_points = PlacementEditor::edgeSamplePoints(m_vizManager->getCurrentStepReader().get(), c.edge_id);
    }
    
    // Update via UIState
    // Command pattern: Update constraint
    auto command = std::make_unique<UpdateConstraintConditionCommand>(
        m_uiState,
        m_currentIndex,
        c
    );
    command->execute();
}

void ConstraintPropertyWidget::setReadOnly(bool readOnly)
{
    m_nameEdit->setReadOnly(readOnly);
    m_surfaceIdEdit->setReadOnly(readOnly);
    m_placementEditor->setReadOnly(readOnly);
    m_readOnlyHintLabel->setVisible(readOnly);

    QString inputStyle;
    if (readOnly) {
        inputStyle = QString("QLineEdit { color: #666666; background-color: #1a1a1a; border: 1px solid #333333; padding: %1px; border-radius: %2px; min-height: %3px; }")
            .arg(StyleManager::PADDING_SMALL)
            .arg(StyleManager::RADIUS_SMALL)
            .arg(StyleManager::INPUT_HEIGHT_SMALL);
    } else {
        inputStyle = QString("QLineEdit { color: %1; background-color: %2; border: 1px solid %3; padding: %4px; border-radius: %5px; min-height: %6px; selection-background-color: #555555; }")
            .arg(ColorManager::INPUT_TEXT_COLOR.name())
            .arg(ColorManager::INPUT_BACKGROUND_COLOR.name())
            .arg(ColorManager::INPUT_BORDER_COLOR.name())
            .arg(StyleManager::PADDING_SMALL)
            .arg(StyleManager::RADIUS_SMALL)
            .arg(StyleManager::INPUT_HEIGHT_SMALL);
    }
    m_nameEdit->setStyleSheet(inputStyle);
    m_surfaceIdEdit->setStyleSheet(inputStyle);
}

void ConstraintPropertyWidget::onEdgeSelected(int edgeId)
{
    if (!m_isSelectingEdge) return;
    m_placementEditor->setEdge(edgeId);
    setEdgeSelecting(false);
    pushData();
}

void ConstraintPropertyWidget::setEdgeSelecting(bool selecting)
{
    m_isSelectingEdge = selecting;
    m_placementEditor->setEdgeSelecting(selecting);
    if (m_vizManager) {
        m_vizManager->setEdgeSelectionMode(selecting);
        // Constraint editing: back to face selection when done
        m_vizManager->setFaceSelectionMode(!selecting);
    }
}

void ConstraintPropertyWidget::onPointModeEnabled()
{
    if (!m_vizManager || !m_uiState || m_currentIndex < 0) return;

    auto bc = m_uiState->getBoundaryCondition();
    if (m_currentIndex >= (int)bc.constraints.size()) return;

    // Constraints created before point mode existed have no point on the face yet
    ConstraintCondition c = bc.constraints[m_currentIndex];
    m_placementEditor->applyTo(c);
    m_placementEditor->setPoint(PlacementEditor::pointOnFaceOrCenter(
        m_vizManager->getCurrentStepReader().get(), c.surface_id, c.point));
}

void ConstraintPropertyWidget::onCloseClicked()
{
    if (m_isSelectingEdge) {
        setEdgeSelecting(false);
    }

    if (m_uiState) {
        m_uiState->setSelectedObject(ObjectType::NONE);
    }
    emit closeClicked();
}
