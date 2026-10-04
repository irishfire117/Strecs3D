#include "ConstraintSetter.h"
#include <gmsh.h>
#include <iostream>
#include <cmath>
#include <set>
#include <algorithm>

ConstraintSetter::ConstraintSetter() {
}

ConstraintSetter::~ConstraintSetter() {
}

void ConstraintSetter::addConstraint(const ConstraintProperties& constraint) {
    constraints_.push_back(constraint);
}

void ConstraintSetter::addConstraint(int surface_number) {
    constraints_.push_back(createConstraintCondition(surface_number));
}

namespace {
double distance(const double* a, const std::array<double, 3>& b) {
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
}

// 形状サイズに対する許容誤差
double geometryTolerance() {
    double xmin, ymin, zmin, xmax, ymax, zmax;
    gmsh::model::getBoundingBox(-1, -1, xmin, ymin, zmin, xmax, ymax, zmax);
    double diag = std::sqrt((xmax - xmin) * (xmax - xmin) + (ymax - ymin) * (ymax - ymin) + (zmax - zmin) * (zmax - zmin));
    return std::max(1e-4, 1e-5 * diag);
}

// 全サンプル点が曲線上（許容誤差内）にあるか
bool curveContainsPoints(int tag, const std::vector<std::array<double, 3>>& points, double tol) {
    for (const auto& p : points) {
        std::vector<double> closest, param;
        gmsh::model::getClosestPoint(1, tag, {p[0], p[1], p[2]}, closest, param);
        if (closest.size() < 3 || distance(closest.data(), p) > tol) {
            return false;
        }
    }
    return true;
}
}

int ConstraintSetter::resolveCurveTag(const ConstraintProperties& constraint) const {
    std::vector<std::pair<int, int>> curves;
    gmsh::model::getEntities(curves, 1);

    // サンプル点がなければ ID をそのまま使う
    if (constraint.edge_points.empty()) {
        for (const auto& c : curves) {
            if (c.second == constraint.edge_number) return c.second;
        }
        return -1;
    }

    double tol = geometryTolerance();
    // UI の edge ID と gmsh の曲線タグは通常一致する（同じ STEP を同じ順序で走査）が、念のため形状で確認
    for (const auto& c : curves) {
        if (c.second == constraint.edge_number && curveContainsPoints(c.second, constraint.edge_points, tol)) {
            return c.second;
        }
    }
    for (const auto& c : curves) {
        if (curveContainsPoints(c.second, constraint.edge_points, tol)) {
            std::cout << "警告: Edge " << constraint.edge_number << " は gmsh の Curve " << c.second
                      << " として見つかりました（形状照合）" << std::endl;
            return c.second;
        }
    }
    return -1;
}

std::vector<std::size_t> ConstraintSetter::getConstraintNodeTags(const ConstraintProperties& constraint) const {
    std::vector<std::size_t> node_tags;
    std::vector<double> coord, parametricCoord;

    switch (constraint.kind) {
    case ConstraintKind::Face:
        gmsh::model::mesh::getNodes(node_tags, coord, parametricCoord, 2, constraint.surface_number, true);
        break;

    case ConstraintKind::Point: {
        std::vector<std::size_t> surface_nodes;
        gmsh::model::mesh::getNodes(surface_nodes, coord, parametricCoord, 2, constraint.surface_number, true);
        std::size_t nearest = 0;
        double nearest_distance = 1e300;
        for (std::size_t i = 0; i < surface_nodes.size(); ++i) {
            double d = distance(&coord[3 * i], constraint.point);
            if (d <= constraint.radius) {
                node_tags.push_back(surface_nodes[i]);
            }
            if (d < nearest_distance) {
                nearest_distance = d;
                nearest = surface_nodes[i];
            }
        }
        double max_element_size = 0.0;
        gmsh::option::getNumber("Mesh.CharacteristicLengthMax", max_element_size);
        if (node_tags.empty() && !surface_nodes.empty() && nearest_distance > constraint.radius + max_element_size) {
            // 拘束点がこの面上にない（入力ミス）: 節点なしとして扱う
            std::cerr << "エラー: 拘束点 (" << constraint.point[0] << ", " << constraint.point[1] << ", "
                      << constraint.point[2] << ") が Surface " << constraint.surface_number
                      << " 上にありません（最も近い節点まで " << nearest_distance << "）。" << std::endl;
        } else if (node_tags.empty() && !surface_nodes.empty()) {
            // 半径がメッシュサイズより小さい場合は最も近い節点を拘束
            node_tags.push_back(nearest);
            std::cout << "警告: Surface " << constraint.surface_number << " の拘束点から半径 " << constraint.radius
                      << " 内に節点がありません。最も近い節点 (距離 " << nearest_distance << ") を拘束します。" << std::endl;
        }
        break;
    }

    case ConstraintKind::Edge: {
        int tag = resolveCurveTag(constraint);
        if (tag < 0) {
            std::cerr << "エラー: Edge " << constraint.edge_number << " に対応する曲線が見つかりません。" << std::endl;
            break;
        }
        gmsh::model::mesh::getNodes(node_tags, coord, parametricCoord, 1, tag, true);
        break;
    }
    }

    return node_tags;
}

std::vector<std::size_t> ConstraintSetter::collectConstraintNodes(const std::vector<ConstraintProperties>& constraints) const {
    // 全拘束の節点を重複なしで1つの節点セットにまとめる
    std::set<std::size_t> all_nodes;
    for (const auto& constraint : constraints) {
        std::vector<std::size_t> node_tags = getConstraintNodeTags(constraint);
        if (node_tags.empty()) {
            // 1つでも節点のない拘束があれば失敗扱い（黙って無視すると支持条件が欠ける）
            return {};
        }
        all_nodes.insert(node_tags.begin(), node_tags.end());

        switch (constraint.kind) {
        case ConstraintKind::Face:
            std::cout << "Surface " << constraint.surface_number << " のノード数: " << node_tags.size() << std::endl;
            break;
        case ConstraintKind::Point:
            std::cout << "Surface " << constraint.surface_number << " の点 (" << constraint.point[0] << ", "
                      << constraint.point[1] << ", " << constraint.point[2] << ") 半径 " << constraint.radius
                      << " のノード数: " << node_tags.size() << std::endl;
            break;
        case ConstraintKind::Edge:
            std::cout << "Edge " << constraint.edge_number << " のノード数: " << node_tags.size() << std::endl;
            break;
        }
    }
    return std::vector<std::size_t>(all_nodes.begin(), all_nodes.end());
}

bool ConstraintSetter::nodesAreCollinear(const std::vector<std::size_t>& node_tags) {
    std::vector<std::array<double, 3>> points;
    for (std::size_t tag : node_tags) {
        std::vector<double> coord, param;
        int dim, entity;
        gmsh::model::mesh::getNode(tag, coord, param, dim, entity);
        points.push_back({coord[0], coord[1], coord[2]});
    }
    if (points.size() < 3) return true;

    // 最初の点から最も遠い点で直線を決め、その直線から最も離れた点までの距離で判定
    const auto& a = points.front();
    const std::array<double, 3>* b = &a;
    for (const auto& p : points) {
        if (distance(p.data(), a) > distance(b->data(), a)) b = &p;
    }
    double length = distance(b->data(), a);
    double tol = 10.0 * geometryTolerance();
    if (length < tol) return true;  // 全て1点に集中

    double u[3] = {((*b)[0] - a[0]) / length, ((*b)[1] - a[1]) / length, ((*b)[2] - a[2]) / length};
    for (const auto& p : points) {
        double v[3] = {p[0] - a[0], p[1] - a[1], p[2] - a[2]};
        double cross[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
        if (std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]) > tol) {
            return false;
        }
    }
    return true;
}

void ConstraintSetter::writeConstraintNodeSet(std::ofstream& f, const std::vector<std::size_t>& node_tags) const {
    f << "***********************************************************\n";
    f << "** constraints fixed node sets\n";
    f << "** ConstraintFixed\n";
    f << "*NSET,NSET=ConstraintFixed\n";
    for (std::size_t tag : node_tags) {
        f << tag << ",\n";
    }
}

void ConstraintSetter::writeFixedConstraints(std::ofstream& f) const {
    f << "***********************************************************\n";
    f << "** Fixed Constraints\n";
    f << "** ConstraintFixed\n";
    f << "*BOUNDARY\n";
    f << "ConstraintFixed,1\n";
    f << "ConstraintFixed,2\n";
    f << "ConstraintFixed,3\n";
}

const std::vector<ConstraintProperties>& ConstraintSetter::getConstraints() const {
    return constraints_;
}

ConstraintProperties createConstraintCondition(int surface_number) {
    ConstraintProperties constraint;
    constraint.surface_number = surface_number;
    return constraint;
}

ConstraintProperties createPointConstraintCondition(int surface_number, const std::array<double, 3>& point, double radius) {
    ConstraintProperties constraint;
    constraint.surface_number = surface_number;
    constraint.kind = ConstraintKind::Point;
    constraint.point = point;
    constraint.radius = radius;
    return constraint;
}

ConstraintProperties createEdgeConstraintCondition(int edge_number, const std::vector<std::array<double, 3>>& edge_points) {
    ConstraintProperties constraint;
    constraint.surface_number = 0;
    constraint.kind = ConstraintKind::Edge;
    constraint.edge_number = edge_number;
    constraint.edge_points = edge_points;
    return constraint;
}
