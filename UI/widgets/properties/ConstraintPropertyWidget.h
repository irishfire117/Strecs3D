#pragma once

#include <QWidget>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include "../../../core/ui/UIState.h"

class VisualizationManager;
class PlacementEditor;

class ConstraintPropertyWidget : public QWidget {
    Q_OBJECT

public:
    explicit ConstraintPropertyWidget(QWidget* parent = nullptr);
    void setUIState(UIState* uiState);
    void setTarget(int index); // Set which constraint to edit
    void setReadOnly(bool readOnly);

    // VisualizationManager for edge selection and face geometry
    void setVisualizationManager(VisualizationManager* vizManager);

signals:
    void closeClicked();

private slots:
    void onEdgeSelected(int edgeId);

private:
    void setupUI();
    void updateData();
    void pushData();
    void onCloseClicked();
    void setEdgeSelecting(bool selecting);
    void onPointModeEnabled();

    UIState* m_uiState = nullptr;
    VisualizationManager* m_vizManager = nullptr;
    PlacementEditor* m_placementEditor;
    bool m_isSelectingEdge = false;
    int m_currentIndex = -1;

    QLineEdit* m_nameEdit;
    QLineEdit* m_surfaceIdEdit;
    QPushButton* m_closeButton;
    QLabel* m_readOnlyHintLabel;
};
