#ifndef MESH_GENERATOR_H
#define MESH_GENERATOR_H

#include <string>
#include <vector>

// generateMesh() return codes besides 0 (success) and 1 (error)
constexpr int MESH_DISCONNECTED_SOLIDS = 2;  // several solids that share no faces (an assembly)

class MeshGenerator {
public:
    MeshGenerator();
    ~MeshGenerator();

    // Generate mesh from STEP file
    int generateMesh(const std::string& step_file);

    // Get available surface tags
    std::vector<int> getSurfaceTags() const;

    // Check if a surface exists
    bool hasSurface(int surface_number) const;

    // Set mesh parameters
    void setCharacteristicLength(double min_length, double max_length);
    void setMeshAlgorithm(int algorithm);
    void setMeshOrder(int order);

    // Refine the mesh within radius of a point (used for patch loads)
    void addRefinementPoint(double x, double y, double z, double radius);

private:
    std::vector<int> surface_tags_;
    double char_length_min_;
    double char_length_max_;
    int mesh_algorithm_;
    int mesh_order_;

    struct RefinementPoint {
        double x, y, z, radius;
    };
    std::vector<RefinementPoint> refinement_points_;

    void applyRefinementFields() const;

    // Set the element order, falling back to straight-sided elements if curving them fails
    void applyHighOrder() const;
    // Smallest scaled Jacobian over all volume elements (<= 0: inverted element)
    static double minScaledJacobian();
    // Number of groups of solids connected through shared faces (1 = a single connected part)
    static int countConnectedSolids();
};

#endif // MESH_GENERATOR_H
