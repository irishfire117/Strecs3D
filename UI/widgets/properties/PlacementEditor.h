#pragma once

#include <QObject>
#include <QPointer>
#include <QComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>
#include "../../../core/types/BoundaryCondition.h"

class StepReader;

class QLabel;

// Where a boundary condition applies (whole face / patch at point + radius / edge) and,
// for loads, its direction (X/Y/Z vector). Adds its rows to an existing QFormLayout so
// they line up with the surrounding fields. Used by the load and constraint dialogs and
// property panels.
class PlacementEditor : public QObject {
    Q_OBJECT

public:
    enum Mode { ModeFace = 0, ModePoint = 1, ModeEdge = 2 };

    struct Options {
        bool direction = true;   // Direction fields + quick buttons (loads)
        bool edge = false;       // "Edge" mode with an edge picker (constraints)
    };

    PlacementEditor(QWidget* parentWidget, QFormLayout* layout, int fieldWidth);
    PlacementEditor(QWidget* parentWidget, QFormLayout* layout, int fieldWidth, Options options);

    // Update fields without emitting signals / write fields into the condition
    void setLoad(const LoadCondition& load);
    void applyTo(LoadCondition& load) const;
    void setConstraint(const ConstraintCondition& constraint);
    void applyTo(ConstraintCondition& constraint) const;

    void setDirection(const Vector3D& direction);
    void setPoint(const Vector3D& point);
    void setEdge(int edgeId);                  // 0 = none
    void setEdgeSelecting(bool selecting);     // Toggle the Select Edge button text
    int mode() const;
    bool usePoint() const;
    void setReadOnly(bool readOnly);

    // Returns point if it lies on the face, otherwise the face center (for a fresh patch load)
    static Vector3D pointOnFaceOrCenter(const StepReader* stepReader, int surfaceId, const Vector3D& point);
    // Direction into the face at the load point (patch) or face center; false if unavailable
    static bool inwardNormal(const StepReader* stepReader, const LoadCondition& load, Vector3D& normal);
    // Sample points along an edge, stored with edge constraints so the FEM side can verify the edge
    static std::vector<Vector3D> edgeSamplePoints(const StepReader* stepReader, int edgeId);

signals:
    void changed();             // User edited position, radius or direction
    void directionEdited();     // User typed a direction or used a quick button
    void pointModeEnabled();    // User switched to "At point"
    void normalRequested();     // User clicked the "Normal" button
    void edgeSelectionRequested();  // User clicked "Select Edge" (owner toggles edge picking)

private:
    QWidget* createVectorRow(QLineEdit* edits[3], int decimals);
    void onModeChanged(int index);
    void onDirectionEditingFinished();
    void setQuickDirection(double x, double y, double z);
    void updateRowVisibility();
    void applyInputStyle(bool readOnly);

    QWidget* m_parent;
    QPointer<QFormLayout> m_layout;  // May be deleted before this object during widget teardown
    Options m_options;

    QComboBox* m_modeCombo;
    QLineEdit* m_pointEdits[3];
    QLineEdit* m_radiusEdit;
    QLineEdit* m_directionEdits[3] = {nullptr, nullptr, nullptr};
    QWidget* m_pointRow;
    QWidget* m_radiusRow;
    QWidget* m_quickButtonRow = nullptr;
    QList<QPushButton*> m_quickButtons;
    QWidget* m_edgeRow = nullptr;
    QPushButton* m_edgeButton = nullptr;
    QLabel* m_edgeLabel = nullptr;
    int m_edgeId = 0;

    Vector3D m_direction = {0.0, 0.0, -1.0};
};
