#pragma once

#include <string>
#include <vector>

// 3次元ベクトル構造体
struct Vector3D {
    double x;
    double y;
    double z;
};

// 拘束の適用対象
enum class ConstraintTarget {
    Face = 0,   // 面全体
    Point = 1,  // 面上の点を中心とした半径 radius の範囲
    Edge = 2    // エッジ全体
};

// 拘束条件構造体
struct ConstraintCondition {
    int surface_id = 0;
    std::string name;

    ConstraintTarget target = ConstraintTarget::Face;
    Vector3D point = {0.0, 0.0, 0.0};  // target == Point
    double radius = 3.0;                // target == Point (mm)
    int edge_id = 0;                    // target == Edge (1-based)
    std::vector<Vector3D> edge_points;  // target == Edge: エッジ上のサンプル点（FEM側でエッジを照合）

    // 適用対象が指定されているか
    bool hasTarget() const {
        return target == ConstraintTarget::Edge ? edge_id > 0 : surface_id > 0;
    }
};

// 荷重条件構造体
struct LoadCondition {
    int surface_id;
    std::string name;
    double magnitude;
    Vector3D direction;
    int reference_edge_id = 0;  // 0 = no edge reference, 1-based edge index

    // 荷重位置: false = 面全体に分布, true = point を中心とした半径 radius の範囲に分布
    bool use_point = false;
    Vector3D point = {0.0, 0.0, 0.0};
    double radius = 3.0;  // mm
};

// 境界条件構造体
struct BoundaryCondition {
    std::vector<ConstraintCondition> constraints;
    std::vector<LoadCondition> loads;
};
