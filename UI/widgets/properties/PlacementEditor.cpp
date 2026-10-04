#include "PlacementEditor.h"
#include "../../../utils/ColorManager.h"
#include "../../../utils/StyleManager.h"
#include "../../../core/processing/StepReader.h"
#include <QDoubleValidator>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <cmath>

namespace {
const char* kLabelStyle = "color: #aaaaaa; background-color: transparent;";
const int kVectorEditWidth = 56;
}

PlacementEditor::PlacementEditor(QWidget* parentWidget, QFormLayout* layout, int fieldWidth)
    : PlacementEditor(parentWidget, layout, fieldWidth, Options())
{
}

PlacementEditor::PlacementEditor(QWidget* parentWidget, QFormLayout* layout, int fieldWidth, Options options)
    : QObject(parentWidget)
    , m_parent(parentWidget)
    , m_layout(layout)
    , m_options(options)
{
    auto addRow = [this](const QString& text, QWidget* field) {
        QLabel* label = new QLabel(text, m_parent);
        label->setStyleSheet(kLabelStyle);
        m_layout->addRow(label, field);
    };

    // Position mode
    m_modeCombo = new QComboBox(m_parent);
    m_modeCombo->addItem("Whole face");
    m_modeCombo->addItem("At point");
    if (m_options.edge) {
        m_modeCombo->addItem("Edge");
    }
    m_modeCombo->setFixedWidth(fieldWidth);
    m_modeCombo->setToolTip(m_options.edge
        ? "Whole face: the entire face.\nAt point: the part of the face within Radius of Point.\nEdge: the whole selected edge."
        : "Whole face: force spread evenly over the face.\nAt point: force spread over the part of the face within Radius of Point.");
    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, &PlacementEditor::onModeChanged);
    addRow("Apply To:", m_modeCombo);

    // Point
    m_pointRow = createVectorRow(m_pointEdits, 3);
    for (QLineEdit* edit : m_pointEdits) {
        edit->setToolTip("Point on the face (mm). Double-click the face to pick it.");
        connect(edit, &QLineEdit::editingFinished, this, &PlacementEditor::changed);
    }
    addRow("Point (mm):", m_pointRow);

    // Radius
    m_radiusEdit = new QLineEdit(m_parent);
    m_radiusEdit->setValidator(new QDoubleValidator(0.01, 1e6, 3, m_radiusEdit));
    m_radiusEdit->setFixedWidth(fieldWidth);
    m_radiusEdit->setToolTip("Radius of the patch around the point (mm)");
    connect(m_radiusEdit, &QLineEdit::editingFinished, this, &PlacementEditor::changed);
    m_radiusRow = m_radiusEdit;
    addRow("Radius (mm):", m_radiusEdit);

    // Edge
    if (m_options.edge) {
        m_edgeRow = new QWidget(m_parent);
        QHBoxLayout* edgeLayout = new QHBoxLayout(m_edgeRow);
        edgeLayout->setContentsMargins(0, 0, 0, 0);
        edgeLayout->setSpacing(5);
        m_edgeButton = new QPushButton("Select Edge", m_edgeRow);
        m_edgeButton->setFixedWidth(100);
        m_edgeButton->setStyleSheet(QString("color: white; background-color: #444; border: 1px solid #666; padding: %1px; border-radius: %2px;")
            .arg(StyleManager::PADDING_MEDIUM)
            .arg(StyleManager::RADIUS_SMALL));
        connect(m_edgeButton, &QPushButton::clicked, this, &PlacementEditor::edgeSelectionRequested);
        edgeLayout->addWidget(m_edgeButton);
        m_edgeLabel = new QLabel("-", m_edgeRow);
        m_edgeLabel->setStyleSheet(kLabelStyle);
        m_edgeLabel->setMinimumWidth(m_edgeLabel->fontMetrics().horizontalAdvance("Edge 0000"));
        edgeLayout->addWidget(m_edgeLabel);
        edgeLayout->addStretch();
        addRow("Edge:", m_edgeRow);
    }

    applyInputStyle(false);
    if (!m_options.direction) {
        setConstraint(ConstraintCondition{});
        return;
    }

    // Direction
    QWidget* directionRow = createVectorRow(m_directionEdits, 3);
    for (QLineEdit* edit : m_directionEdits) {
        edit->setToolTip("Load direction (normalized automatically)");
        connect(edit, &QLineEdit::editingFinished, this, &PlacementEditor::onDirectionEditingFinished);
    }
    addRow("Direction:", directionRow);

    // Quick direction buttons
    m_quickButtonRow = new QWidget(m_parent);
    QGridLayout* grid = new QGridLayout(m_quickButtonRow);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(4);
    grid->setVerticalSpacing(4);
    QString buttonStyle = QString(
        "QPushButton { color: white; background-color: #444; border: 1px solid #666; padding: 2px; border-radius: %1px; }"
        "QPushButton:hover { background-color: #555; }"
        "QPushButton:disabled { color: #666; }")
        .arg(StyleManager::RADIUS_SMALL);
    struct Quick { const char* text; int row; int col; double x, y, z; };
    const Quick quicks[] = {
        {"+X", 0, 0, 1, 0, 0}, {"+Y", 0, 1, 0, 1, 0}, {"+Z", 0, 2, 0, 0, 1},
        {"-X", 1, 0, -1, 0, 0}, {"-Y", 1, 1, 0, -1, 0}, {"-Z", 1, 2, 0, 0, -1},
    };
    for (const Quick& q : quicks) {
        QPushButton* button = new QPushButton(q.text, m_quickButtonRow);
        button->setStyleSheet(buttonStyle);
        button->setFixedWidth(kVectorEditWidth);
        double x = q.x, y = q.y, z = q.z;
        connect(button, &QPushButton::clicked, this, [this, x, y, z]() { setQuickDirection(x, y, z); });
        grid->addWidget(button, q.row, q.col);
        m_quickButtons.append(button);
    }
    QPushButton* normalButton = new QPushButton("Normal", m_quickButtonRow);
    normalButton->setStyleSheet(buttonStyle);
    normalButton->setToolTip("Point into the face, perpendicular to it");
    connect(normalButton, &QPushButton::clicked, this, &PlacementEditor::normalRequested);
    grid->addWidget(normalButton, 0, 3, 2, 1);
    normalButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    m_quickButtons.append(normalButton);
    m_layout->addRow(QString(), m_quickButtonRow);

    applyInputStyle(false);
    setLoad(LoadCondition{});
}

void PlacementEditor::setConstraint(const ConstraintCondition& constraint)
{
    {
        QSignalBlocker blocker(m_modeCombo);
        int index = static_cast<int>(constraint.target);
        m_modeCombo->setCurrentIndex(index < m_modeCombo->count() ? index : 0);
    }
    setPoint(constraint.point);
    {
        QSignalBlocker blocker(m_radiusEdit);
        m_radiusEdit->setText(QString::number(constraint.radius, 'f', 2));
    }
    setEdge(constraint.edge_id);
    updateRowVisibility();
}

void PlacementEditor::applyTo(ConstraintCondition& constraint) const
{
    constraint.target = static_cast<ConstraintTarget>(mode());
    constraint.point = {m_pointEdits[0]->text().toDouble(),
                        m_pointEdits[1]->text().toDouble(),
                        m_pointEdits[2]->text().toDouble()};
    double radius = m_radiusEdit->text().toDouble();
    if (radius > 0.0) {
        constraint.radius = radius;
    }
    constraint.edge_id = m_edgeId;
}

void PlacementEditor::setEdge(int edgeId)
{
    m_edgeId = edgeId;
    if (m_edgeLabel) {
        m_edgeLabel->setText(edgeId > 0 ? QString("Edge %1").arg(edgeId) : QString("-"));
    }
}

void PlacementEditor::setEdgeSelecting(bool selecting)
{
    if (m_edgeButton) {
        m_edgeButton->setText(selecting ? "Cancel" : "Select Edge");
    }
}

int PlacementEditor::mode() const
{
    return m_modeCombo->currentIndex();
}

QWidget* PlacementEditor::createVectorRow(QLineEdit* edits[3], int decimals)
{
    QWidget* row = new QWidget(m_parent);
    QHBoxLayout* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    const char* axes[3] = {"X", "Y", "Z"};
    for (int i = 0; i < 3; ++i) {
        edits[i] = new QLineEdit(row);
        edits[i]->setValidator(new QDoubleValidator(-1e9, 1e9, decimals, edits[i]));
        edits[i]->setPlaceholderText(axes[i]);
        edits[i]->setFixedWidth(kVectorEditWidth);
        layout->addWidget(edits[i]);
    }
    layout->addStretch();
    return row;
}

void PlacementEditor::setLoad(const LoadCondition& load)
{
    {
        QSignalBlocker blocker(m_modeCombo);
        m_modeCombo->setCurrentIndex(load.use_point ? 1 : 0);
    }
    setPoint(load.point);
    {
        QSignalBlocker blocker(m_radiusEdit);
        m_radiusEdit->setText(QString::number(load.radius, 'f', 2));
    }
    setDirection(load.direction);
    updateRowVisibility();
}

void PlacementEditor::applyTo(LoadCondition& load) const
{
    load.use_point = usePoint();
    load.point = {m_pointEdits[0]->text().toDouble(),
                  m_pointEdits[1]->text().toDouble(),
                  m_pointEdits[2]->text().toDouble()};
    double radius = m_radiusEdit->text().toDouble();
    if (radius > 0.0) {
        load.radius = radius;
    }
    load.direction = m_direction;
}

void PlacementEditor::setDirection(const Vector3D& direction)
{
    m_direction = direction;
    if (!m_options.direction) return;

    // Leave the fields alone if they already describe this direction (e.g. the typed
    // (0, 1, -1) when the state refreshes with (0, 0.707, -0.707))
    Vector3D typed = {m_directionEdits[0]->text().toDouble(),
                      m_directionEdits[1]->text().toDouble(),
                      m_directionEdits[2]->text().toDouble()};
    double typedLength = std::sqrt(typed.x * typed.x + typed.y * typed.y + typed.z * typed.z);
    double length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (typedLength > 1e-9 && length > 1e-9 &&
        std::abs(typed.x / typedLength - direction.x / length) < 1e-6 &&
        std::abs(typed.y / typedLength - direction.y / length) < 1e-6 &&
        std::abs(typed.z / typedLength - direction.z / length) < 1e-6) {
        return;
    }

    const double values[3] = {direction.x, direction.y, direction.z};
    for (int i = 0; i < 3; ++i) {
        QSignalBlocker blocker(m_directionEdits[i]);
        m_directionEdits[i]->setText(QString::number(values[i], 'f', 3));
    }
}

void PlacementEditor::setPoint(const Vector3D& point)
{
    const double values[3] = {point.x, point.y, point.z};
    for (int i = 0; i < 3; ++i) {
        QSignalBlocker blocker(m_pointEdits[i]);
        m_pointEdits[i]->setText(QString::number(values[i], 'f', 2));
    }
}

bool PlacementEditor::usePoint() const
{
    return m_modeCombo->currentIndex() == ModePoint;
}

void PlacementEditor::setReadOnly(bool readOnly)
{
    m_modeCombo->setEnabled(!readOnly);
    for (int i = 0; i < 3; ++i) {
        m_pointEdits[i]->setReadOnly(readOnly);
        if (m_directionEdits[i]) m_directionEdits[i]->setReadOnly(readOnly);
    }
    m_radiusEdit->setReadOnly(readOnly);
    for (QPushButton* button : m_quickButtons) {
        button->setEnabled(!readOnly);
    }
    if (m_edgeButton) {
        m_edgeButton->setEnabled(!readOnly);
    }
    applyInputStyle(readOnly);
}

void PlacementEditor::onModeChanged(int index)
{
    // QComboBox emits -1 while it is being destroyed
    if (index < 0) return;

    updateRowVisibility();
    if (index == ModePoint) {
        emit pointModeEnabled();
    }
    emit changed();
}

void PlacementEditor::onDirectionEditingFinished()
{
    Vector3D d = {m_directionEdits[0]->text().toDouble(),
                  m_directionEdits[1]->text().toDouble(),
                  m_directionEdits[2]->text().toDouble()};
    double length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    if (length < 1e-9) {
        // Zero vector is not a direction: restore the previous one
        setDirection(m_direction);
        return;
    }
    // Keep the typed components in the fields (rewriting them would mix normalized and
    // raw values while the user edits X, Y, Z one at a time); normalize internally.
    Vector3D normalized = {d.x / length, d.y / length, d.z / length};
    bool unchanged = std::abs(normalized.x - m_direction.x) < 1e-9 &&
                     std::abs(normalized.y - m_direction.y) < 1e-9 &&
                     std::abs(normalized.z - m_direction.z) < 1e-9;
    m_direction = normalized;
    if (!unchanged) {
        emit directionEdited();
        emit changed();
    }
}

void PlacementEditor::setQuickDirection(double x, double y, double z)
{
    setDirection({x, y, z});
    emit directionEdited();
    emit changed();
}

void PlacementEditor::updateRowVisibility()
{
    if (!m_layout) return;

    bool patch = usePoint();
    m_layout->setRowVisible(m_pointRow, patch);
    m_layout->setRowVisible(m_radiusRow, patch);
    if (m_edgeRow) {
        m_layout->setRowVisible(m_edgeRow, mode() == ModeEdge);
    }
}

void PlacementEditor::applyInputStyle(bool readOnly)
{
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
    for (int i = 0; i < 3; ++i) {
        m_pointEdits[i]->setStyleSheet(inputStyle);
        if (m_directionEdits[i]) m_directionEdits[i]->setStyleSheet(inputStyle);
    }
    m_radiusEdit->setStyleSheet(inputStyle);
    m_modeCombo->setStyleSheet(QString("QComboBox { color: %1; background-color: %2; border: 1px solid %3; padding: %4px; border-radius: %5px; }")
        .arg(readOnly ? QString("#666666") : ColorManager::INPUT_TEXT_COLOR.name())
        .arg(readOnly ? QString("#1a1a1a") : ColorManager::INPUT_BACKGROUND_COLOR.name())
        .arg(readOnly ? QString("#333333") : ColorManager::INPUT_BORDER_COLOR.name())
        .arg(StyleManager::PADDING_SMALL)
        .arg(StyleManager::RADIUS_SMALL));
}

Vector3D PlacementEditor::pointOnFaceOrCenter(const StepReader* stepReader, int surfaceId, const Vector3D& point)
{
    if (!stepReader || surfaceId <= 0) return point;

    FaceGeometry projected = stepReader->getFaceGeometryAtPoint(surfaceId, point.x, point.y, point.z);
    if (projected.isValid) {
        double dx = projected.centerX - point.x;
        double dy = projected.centerY - point.y;
        double dz = projected.centerZ - point.z;
        if (std::sqrt(dx * dx + dy * dy + dz * dz) <= 0.5) {
            return point;
        }
    }
    FaceGeometry center = stepReader->getFaceGeometry(surfaceId);
    if (!center.isValid) return point;
    return {center.centerX, center.centerY, center.centerZ};
}

bool PlacementEditor::inwardNormal(const StepReader* stepReader, const LoadCondition& load, Vector3D& normal)
{
    if (!stepReader || load.surface_id <= 0) return false;

    FaceGeometry geom = load.use_point
        ? stepReader->getFaceGeometryAtPoint(load.surface_id, load.point.x, load.point.y, load.point.z)
        : stepReader->getFaceGeometry(load.surface_id);
    if (!geom.isValid) return false;

    // Into the face (same convention as double-clicking a face)
    normal = {-geom.normalX, -geom.normalY, -geom.normalZ};
    return true;
}

std::vector<Vector3D> PlacementEditor::edgeSamplePoints(const StepReader* stepReader, int edgeId)
{
    std::vector<Vector3D> points;
    if (!stepReader || edgeId <= 0) return points;
    for (const auto& p : stepReader->getEdgeSamplePoints(edgeId, 5)) {
        points.push_back({p[0], p[1], p[2]});
    }
    return points;
}
