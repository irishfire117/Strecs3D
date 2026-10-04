#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include "../../../core/types/BoundaryCondition.h"

class VisualizationManager;
class PlacementEditor;

class AddConstraintDialog : public QDialog {
    Q_OBJECT

public:
    explicit AddConstraintDialog(const QString& defaultName, QWidget* parent = nullptr);
    ~AddConstraintDialog();

    void setVisualizationManager(VisualizationManager* vizManager);
    ConstraintCondition getConstraintCondition() const;

private slots:
    void onFaceDoubleClicked(int faceId, double nx, double ny, double nz);
    void onEdgeSelected(int edgeId);

private:
    void setupUI();
    void enableFaceSelectionMode(bool enable);
    void setEdgeSelecting(bool selecting);
    void updatePreview();
    void onPointModeEnabled();

    QLineEdit* m_nameEdit;
    QLineEdit* m_surfaceIdEdit;
    PlacementEditor* m_placementEditor;
    bool m_isSelectingEdge = false;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;

    VisualizationManager* m_vizManager = nullptr;
};
