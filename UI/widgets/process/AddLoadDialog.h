#pragma once

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include "../../../core/types/BoundaryCondition.h"

class VisualizationManager;
class StepReader;
class PlacementEditor;

class AddLoadDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddLoadDialog(const QString& defaultName, QWidget* parent = nullptr);
    ~AddLoadDialog();

    void setVisualizationManager(VisualizationManager* vizManager);
    LoadCondition getLoadCondition() const;

private slots:
    void onFaceDoubleClicked(int faceId, double nx, double ny, double nz);
    void onEdgeSelected(int edgeId);
    void onReferenceEdgeButtonClicked();
    void onReverseDirectionToggled(bool checked);

private:
    void setupUI();
    void enableFaceSelectionMode(bool enable);
    void cancelEdgeSelection();
    void updateDirectionFromEdge(int edgeId);
    void updateDirectionDisplay();
    void updatePreview();
    void onPlacementChanged();
    void onDirectionEdited();
    void onPointModeEnabled();
    void onNormalRequested();

    QLineEdit* m_nameEdit;
    QLineEdit* m_surfaceIdEdit;
    QLineEdit* m_magnitudeEdit;

    // Edge selection UI
    QPushButton* m_referenceEdgeButton;
    QCheckBox* m_reverseCheckBox;
    QLabel* m_selectedEdgeLabel;
    PlacementEditor* m_placementEditor;

    QPushButton* m_okButton;
    QPushButton* m_cancelButton;

    VisualizationManager* m_vizManager = nullptr;
    bool m_isSelectingEdge = false;

    // Store current load data
    LoadCondition m_currentLoad;
};
