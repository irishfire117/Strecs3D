#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct Vector3D {
    double x;
    double y; 
    double z;
};

struct MeshConfig {
    int min_element_size;
    int max_element_size;
};

struct FixedFace {
    int surface_id = 0;
    std::string name;
    // "face" (whole face), "point" (face within radius of point) or "edge"
    std::string target = "face";
    Vector3D point = {0.0, 0.0, 0.0};
    double radius = 0.0;
    int edge_id = 0;
    std::vector<Vector3D> edge_points;
};

struct AppliedLoad {
    int surface_id = 0;
    std::string name;
    double magnitude = 0.0;
    Vector3D direction = {0.0, 0.0, 0.0};
    // Optional patch load: distribute over the face within `radius` of `point`
    bool use_point = false;
    Vector3D point = {0.0, 0.0, 0.0};
    double radius = 0.0;
};

struct ConstraintsConfig {
    std::vector<FixedFace> fixed_faces;
};

struct LoadsConfig {
    std::vector<AppliedLoad> applied_loads;
};

struct SimulationConfig {
    std::string step_file;
    MeshConfig mesh;
    ConstraintsConfig constraints;
    LoadsConfig loads;
    
    static SimulationConfig fromJsonFile(const std::string& filename);
    static SimulationConfig fromJson(const nlohmann::json& json);
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vector3D, x, y, z)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MeshConfig, min_element_size, max_element_size)
// WITH_DEFAULT so configs written before target/point/radius/edge existed still load
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(FixedFace, surface_id, name, target, point, radius, edge_id, edge_points)
// WITH_DEFAULT so configs written before use_point/point/radius existed still load
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(AppliedLoad, surface_id, name, magnitude, direction, use_point, point, radius)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ConstraintsConfig, fixed_faces)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LoadsConfig, applied_loads)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SimulationConfig, step_file, mesh, constraints, loads)