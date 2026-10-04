#ifndef CONSTRAINT_SETTER_H
#define CONSTRAINT_SETTER_H

#include <array>
#include <cstddef>
#include <vector>
#include <fstream>

enum class ConstraintKind {
    Face,   // all nodes of the surface
    Point,  // surface nodes within radius of point
    Edge    // all nodes of the curve
};

struct ConstraintProperties {
    int surface_number;
    ConstraintKind kind = ConstraintKind::Face;
    std::array<double, 3> point = {0.0, 0.0, 0.0};
    double radius = 0.0;
    int edge_number = 0;
    std::vector<std::array<double, 3>> edge_points;  // samples used to verify/find the gmsh curve
};

class ConstraintSetter {
public:
    ConstraintSetter();
    ~ConstraintSetter();

    // Add constraint condition
    void addConstraint(const ConstraintProperties& constraint);
    void addConstraint(int surface_number);

    // Get node tags for a constraint (face, point patch or edge)
    std::vector<std::size_t> getConstraintNodeTags(const ConstraintProperties& constraint) const;

    // Nodes of all constraints, without duplicates; empty if any constraint has no nodes
    std::vector<std::size_t> collectConstraintNodes(const std::vector<ConstraintProperties>& constraints) const;

    // True if the nodes lie on one line (or are a single point): fixing only their
    // translations leaves the part free to rotate about that line
    static bool nodesAreCollinear(const std::vector<std::size_t>& node_tags);

    // Write one node set containing the given nodes
    void writeConstraintNodeSet(std::ofstream& f, const std::vector<std::size_t>& node_tags) const;
    void writeFixedConstraints(std::ofstream& f) const;

    // Get all constraints
    const std::vector<ConstraintProperties>& getConstraints() const;

private:
    // gmsh curve tag for an edge constraint (verified against edge_points), -1 if not found
    int resolveCurveTag(const ConstraintProperties& constraint) const;

    std::vector<ConstraintProperties> constraints_;
};

// Utility functions
ConstraintProperties createConstraintCondition(int surface_number);
ConstraintProperties createPointConstraintCondition(int surface_number, const std::array<double, 3>& point, double radius);
ConstraintProperties createEdgeConstraintCondition(int edge_number, const std::vector<std::array<double, 3>>& edge_points);

#endif // CONSTRAINT_SETTER_H
