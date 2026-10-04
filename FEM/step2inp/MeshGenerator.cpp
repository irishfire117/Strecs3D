#include "MeshGenerator.h"
#include <gmsh.h>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <map>

MeshGenerator::MeshGenerator()
    : char_length_min_(1.0)
    , char_length_max_(5.0)
    , mesh_algorithm_(1)  
    , mesh_order_(2)
{
}

MeshGenerator::~MeshGenerator() {
}

void MeshGenerator::setCharacteristicLength(double min_length, double max_length) {
    char_length_min_ = min_length;
    char_length_max_ = max_length;
}

void MeshGenerator::setMeshAlgorithm(int algorithm) {
    mesh_algorithm_ = algorithm;
}

void MeshGenerator::setMeshOrder(int order) {
    mesh_order_ = order;
}

void MeshGenerator::addRefinementPoint(double x, double y, double z, double radius) {
    refinement_points_.push_back({x, y, z, radius});
}

int MeshGenerator::countConnectedSolids() {
    std::vector<std::pair<int, int>> vols;
    gmsh::model::getEntities(vols, 3);

    // 共有する面でつながったソリッドをまとめる（Union-Find）
    std::vector<int> parent(vols.size());
    for (size_t i = 0; i < parent.size(); ++i) parent[i] = static_cast<int>(i);
    std::function<int(int)> find = [&](int i) { return parent[i] == i ? i : parent[i] = find(parent[i]); };

    std::map<int, int> surface_owner;  // surface tag -> first volume index
    for (size_t i = 0; i < vols.size(); ++i) {
        std::vector<std::pair<int, int>> boundary;
        gmsh::model::getBoundary({vols[i]}, boundary, false, false, false);
        for (const auto& s : boundary) {
            auto it = surface_owner.find(std::abs(s.second));
            if (it == surface_owner.end()) {
                surface_owner[std::abs(s.second)] = static_cast<int>(i);
            } else {
                parent[find(static_cast<int>(i))] = find(it->second);
            }
        }
    }

    int components = 0;
    for (size_t i = 0; i < vols.size(); ++i) {
        if (find(static_cast<int>(i)) == static_cast<int>(i)) ++components;
    }
    return components;
}

double MeshGenerator::minScaledJacobian() {
    std::vector<int> types;
    std::vector<std::vector<std::size_t>> tags, nodes;
    gmsh::model::mesh::getElements(types, tags, nodes, 3, -1);
    double min_sj = 1.0;
    for (const auto& type_tags : tags) {
        if (type_tags.empty()) continue;
        std::vector<double> qualities;
        gmsh::model::mesh::getElementQualities(type_tags, qualities, "minSJ");
        for (double q : qualities) min_sj = std::min(min_sj, q);
    }
    return min_sj;
}

void MeshGenerator::applyHighOrder() const {
    // 二次要素を形状に沿って曲げる。形状によっては曲げた要素の最適化に失敗し、gmsh は既定
    // (API: General.AbortOnError = 2) で OpenMP 並列領域内から例外を投げてプロセスごと終了する
    // ため、この間はエラーをログのみにし、要素の品質で成否を判定する。
    double abort_on_error = 2;
    gmsh::option::getNumber("General.AbortOnError", abort_on_error);
    gmsh::option::setNumber("General.AbortOnError", 0);

    gmsh::model::mesh::setOrder(mesh_order_);
    gmsh::model::mesh::optimize("HighOrder");

    if (mesh_order_ > 1 && minScaledJacobian() <= 0.0) {
        // 反転した曲がり要素が残った: 中間節点を直線上に置いた二次要素にする（一次メッシュが有効なら常に有効）
        std::cout << "警告: 曲面に沿った二次要素の最適化に失敗したため、直線辺の二次要素を使用します。" << std::endl;
        gmsh::model::mesh::setOrder(1);
        gmsh::option::setNumber("Mesh.HighOrderOptimize", 0);
        gmsh::option::setNumber("Mesh.SecondOrderLinear", 1);
        gmsh::model::mesh::setOrder(mesh_order_);
        std::cout << "  最小スケールドヤコビアン: " << minScaledJacobian() << std::endl;
    }

    gmsh::option::setNumber("General.AbortOnError", abort_on_error);
}

void MeshGenerator::applyRefinementFields() const {
    if (refinement_points_.empty()) return;

    // 荷重パッチ周辺を細かくする: パッチ内に複数要素が入るよう半径の1/3程度のサイズに
    std::vector<double> ball_fields;
    for (const auto& p : refinement_points_) {
        double size = std::clamp(p.radius / 3.0, char_length_min_, char_length_max_);
        int field = gmsh::model::mesh::field::add("Ball");
        gmsh::model::mesh::field::setNumber(field, "XCenter", p.x);
        gmsh::model::mesh::field::setNumber(field, "YCenter", p.y);
        gmsh::model::mesh::field::setNumber(field, "ZCenter", p.z);
        gmsh::model::mesh::field::setNumber(field, "Radius", p.radius * 1.5);
        gmsh::model::mesh::field::setNumber(field, "Thickness", std::max(p.radius, size));
        gmsh::model::mesh::field::setNumber(field, "VIn", size);
        gmsh::model::mesh::field::setNumber(field, "VOut", char_length_max_);
        ball_fields.push_back(field);
        std::cout << "荷重点周辺のメッシュを細分化: (" << p.x << ", " << p.y << ", " << p.z
                  << ") 半径 " << p.radius << " サイズ " << size << std::endl;
    }

    int min_field = gmsh::model::mesh::field::add("Min");
    gmsh::model::mesh::field::setNumbers(min_field, "FieldsList", ball_fields);
    gmsh::model::mesh::field::setAsBackgroundMesh(min_field);
}

int MeshGenerator::generateMesh(const std::string& step_file) {
    try {
        std::cout << "STEPファイルを読み込み中: " << step_file << std::endl;
        gmsh::open(step_file);

        gmsh::model::geo::synchronize();

        // Check if volumes exist
        std::vector<std::pair<int, int>> vols;
        gmsh::model::getEntities(vols, 3);

        if (vols.empty()) {
            std::cerr << "エラー: STEPファイル内にボリュームが見つかりませんでした。" << std::endl;
            return 1;
        }

        // 面を共有しない複数のソリッド（アセンブリ）は互いにつながっておらず解析できない
        int components = countConnectedSolids();
        if (components > 1) {
            std::cerr << "エラー: 面を共有しない " << components << " 個のソリッドがあります（アセンブリ）。"
                      << "単一のソリッドが必要です。" << std::endl;
            return MESH_DISCONNECTED_SOLIDS;
        }

        // Add physical group for volumes
        std::vector<int> vol_tags;
        for (const auto& vol : vols) {
            vol_tags.push_back(vol.second);
        }
        gmsh::model::addPhysicalGroup(3, vol_tags, -1, "SolidVolume");

        // Set mesh parameters
        gmsh::option::setNumber("Mesh.CharacteristicLengthMin", char_length_min_);
        gmsh::option::setNumber("Mesh.CharacteristicLengthMax", char_length_max_);
        gmsh::option::setNumber("Mesh.HighOrderOptimize", 2);
        applyRefinementFields();

        // Generate 3D mesh
        std::cout << "3Dメッシュを生成中..." << std::endl;
        gmsh::option::setNumber("Mesh.Algorithm3D", mesh_algorithm_);
        gmsh::model::mesh::generate(3);
        applyHighOrder();

        gmsh::option::setNumber("Mesh.SaveAll", 0);

        // Get surface tags
        std::vector<std::pair<int, int>> surfaces;
        gmsh::model::getEntities(surfaces, 2);

        surface_tags_.clear();
        for (const auto& surface : surfaces) {
            surface_tags_.push_back(surface.second);
        }

        std::cout << "利用可能な面 (Surface):" << std::endl;
        for (int tag : surface_tags_) {
            std::cout << "  Surface " << tag << std::endl;
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "メッシュ生成エラー: " << e.what() << std::endl;
        return 1;
    }
}

std::vector<int> MeshGenerator::getSurfaceTags() const {
    return surface_tags_;
}

bool MeshGenerator::hasSurface(int surface_number) const {
    return std::find(surface_tags_.begin(), surface_tags_.end(), surface_number) != surface_tags_.end();
}
