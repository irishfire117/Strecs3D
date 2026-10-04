#include "LoadConditionSetter.h"
#include <gmsh.h>
#include <iostream>
#include <cmath>
#include <map>
#include <iomanip>

LoadConditionSetter::LoadConditionSetter() {
}

LoadConditionSetter::~LoadConditionSetter() {
}

void LoadConditionSetter::addLoad(const LoadProperties& load) {
    loads_.push_back(load);
}

void LoadConditionSetter::addLoad(int surface_number, double magnitude, const std::vector<double>& direction) {
    loads_.push_back({surface_number, magnitude, direction});
}

double LoadConditionSetter::calculateElementArea(const std::vector<std::vector<double>>& coords) {
    int n_nodes = coords.size();

    // 二次要素 (6節点三角形 / 8,9節点四角形) は頂点節点のみで面積を計算
    // （中間節点を含めて扇形分割すると面積が誤る）
    if (n_nodes == 6) {
        return calculateElementArea({coords[0], coords[1], coords[2]});
    }
    if (n_nodes == 8 || n_nodes == 9) {
        return calculateElementArea({coords[0], coords[1], coords[2], coords[3]});
    }

    if (n_nodes == 3) {
        // 三角形要素: 2つの辺ベクトルの外積の大きさの半分
        std::vector<double> v1 = {coords[1][0] - coords[0][0], coords[1][1] - coords[0][1], coords[1][2] - coords[0][2]};
        std::vector<double> v2 = {coords[2][0] - coords[0][0], coords[2][1] - coords[0][1], coords[2][2] - coords[0][2]};

        // 外積計算
        std::vector<double> cross = {
            v1[1] * v2[2] - v1[2] * v2[1],
            v1[2] * v2[0] - v1[0] * v2[2],
            v1[0] * v2[1] - v1[1] * v2[0]
        };

        // ベクトルの大きさ
        double magnitude = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
        return 0.5 * magnitude;

    } else if (n_nodes == 4) {
        // 四角形要素: 2つの三角形に分割して面積を合計
        std::vector<double> v1 = {coords[1][0] - coords[0][0], coords[1][1] - coords[0][1], coords[1][2] - coords[0][2]};
        std::vector<double> v2 = {coords[2][0] - coords[0][0], coords[2][1] - coords[0][1], coords[2][2] - coords[0][2]};

        std::vector<double> cross1 = {
            v1[1] * v2[2] - v1[2] * v2[1],
            v1[2] * v2[0] - v1[0] * v2[2],
            v1[0] * v2[1] - v1[1] * v2[0]
        };
        double area1 = 0.5 * std::sqrt(cross1[0] * cross1[0] + cross1[1] * cross1[1] + cross1[2] * cross1[2]);

        std::vector<double> v3 = {coords[2][0] - coords[0][0], coords[2][1] - coords[0][1], coords[2][2] - coords[0][2]};
        std::vector<double> v4 = {coords[3][0] - coords[0][0], coords[3][1] - coords[0][1], coords[3][2] - coords[0][2]};

        std::vector<double> cross2 = {
            v3[1] * v4[2] - v3[2] * v4[1],
            v3[2] * v4[0] - v3[0] * v4[2],
            v3[0] * v4[1] - v3[1] * v4[0]
        };
        double area2 = 0.5 * std::sqrt(cross2[0] * cross2[0] + cross2[1] * cross2[1] + cross2[2] * cross2[2]);

        return area1 + area2;

    } else {
        // その他の多角形要素（簡易的に扇形分割）
        double area = 0.0;
        for (int i = 1; i < n_nodes - 1; ++i) {
            std::vector<double> v1 = {coords[i][0] - coords[0][0], coords[i][1] - coords[0][1], coords[i][2] - coords[0][2]};
            std::vector<double> v2 = {coords[i+1][0] - coords[0][0], coords[i+1][1] - coords[0][1], coords[i+1][2] - coords[0][2]};

            std::vector<double> cross = {
                v1[1] * v2[2] - v1[2] * v2[1],
                v1[2] * v2[0] - v1[0] * v2[2],
                v1[0] * v2[1] - v1[1] * v2[0]
            };

            double magnitude = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
            area += 0.5 * magnitude;
        }
        return area;
    }
}

bool LoadConditionSetter::writeForceBoundaryCondition(std::ofstream& f, const LoadProperties& load) const {
    const int surface_number = load.surface_number;
    const double total_force = load.magnitude;
    const std::vector<double>& force_direction = load.direction;

    f << "***********************************************************\n";
    f << "** constraints force node loads\n";
    f << "*CLOAD\n";
    f << "** ConstraintForce\n";
    f << "** node loads on shape: Part__Feature:Face" << surface_number << "\n";
    f << "** Total force: " << total_force << " N, Direction: ["
      << force_direction[0] << ", " << force_direction[1] << ", " << force_direction[2] << "]\n";
    if (load.use_point) {
        f << "** Patch load at [" << load.point[0] << ", " << load.point[1] << ", " << load.point[2]
          << "], radius " << load.radius << "\n";
    }

    // ステップ1: 面上の全要素について節点・面積・重心を収集
    struct SurfaceElement {
        std::vector<std::size_t> node_tags;
        double area;
        double distance;         // 荷重点から要素重心までの距離（パッチ荷重時）
        double inside_fraction;  // 半径内にある面積の割合（パッチ荷重時）
    };
    std::vector<SurfaceElement> surface_elements;

    // 指定された面の要素タグとノードタグを取得
    std::vector<int> element_types;
    std::vector<std::vector<std::size_t>> element_tags;  // 要素番号
    std::vector<std::vector<std::size_t>> node_tags;     // 節点番号
    gmsh::model::mesh::getElements(element_types, element_tags, node_tags, 2, surface_number);

    for (size_t i = 0; i < element_types.size(); ++i) {
        for (std::size_t element_tag : element_tags[i]) {
            // 要素の節点タグを取得
            std::vector<std::size_t> element_node_tags;
            int element_type_out, entity_dim, entity_tag;
            gmsh::model::mesh::getElement(element_tag, element_type_out, element_node_tags, entity_dim, entity_tag);

            // 節点の座標を取得
            std::vector<std::vector<double>> coords;
            double centroid[3] = {0.0, 0.0, 0.0};
            for (std::size_t node_tag : element_node_tags) {
                std::vector<double> coord;
                std::vector<double> parametric_coord;
                int node_dim, node_tag_entity;
                gmsh::model::mesh::getNode(node_tag, coord, parametric_coord, node_dim, node_tag_entity);
                coords.push_back({coord[0], coord[1], coord[2]});
                centroid[0] += coord[0];
                centroid[1] += coord[1];
                centroid[2] += coord[2];
            }

            double distance = 0.0;
            double inside_fraction = 1.0;
            if (load.use_point && !coords.empty()) {
                auto distToPoint = [&load](double x, double y, double z) {
                    double dx = x - load.point[0];
                    double dy = y - load.point[1];
                    double dz = z - load.point[2];
                    return std::sqrt(dx * dx + dy * dy + dz * dz);
                };
                distance = distToPoint(centroid[0] / coords.size(), centroid[1] / coords.size(),
                                       centroid[2] / coords.size());

                if (coords.size() >= 3) {
                    // 三角形を n*n 個の小三角形に分割し、重心が半径内にある割合を面積比とする
                    // （頂点節点のみ使用。二次要素の中間節点は無視。四角形は2つの三角形として扱う）
                    const int n = 6;
                    auto insideFraction = [&](const std::vector<double>& a, const std::vector<double>& b,
                                              const std::vector<double>& c) {
                        int inside = 0;
                        auto sample = [&](double u, double v) {
                            double x = a[0] + u * (b[0] - a[0]) + v * (c[0] - a[0]);
                            double y = a[1] + u * (b[1] - a[1]) + v * (c[1] - a[1]);
                            double z = a[2] + u * (b[2] - a[2]) + v * (c[2] - a[2]);
                            if (distToPoint(x, y, z) <= load.radius) ++inside;
                        };
                        for (int i = 0; i < n; ++i) {
                            for (int j = 0; i + j < n; ++j) {
                                sample((i + 1.0 / 3.0) / n, (j + 1.0 / 3.0) / n);
                                if (i + j < n - 1) {
                                    sample((i + 2.0 / 3.0) / n, (j + 2.0 / 3.0) / n);
                                }
                            }
                        }
                        return static_cast<double>(inside) / (n * n);
                    };
                    bool quad = coords.size() == 4 || coords.size() == 8 || coords.size() == 9;
                    if (quad) {
                        double area1 = calculateElementArea({coords[0], coords[1], coords[2]});
                        double area2 = calculateElementArea({coords[0], coords[2], coords[3]});
                        double total = area1 + area2;
                        inside_fraction = total > 0.0
                            ? (area1 * insideFraction(coords[0], coords[1], coords[2]) +
                               area2 * insideFraction(coords[0], coords[2], coords[3])) / total
                            : 0.0;
                    } else {
                        inside_fraction = insideFraction(coords[0], coords[1], coords[2]);
                    }
                } else {
                    inside_fraction = (distance <= load.radius) ? 1.0 : 0.0;
                }
            }

            surface_elements.push_back({element_node_tags, calculateElementArea(coords), distance, inside_fraction});
        }
    }

    // ステップ2: 荷重を受ける要素を選択（面全体 or 荷重点から半径内）
    std::vector<std::pair<const SurfaceElement*, double>> loaded_elements;  // (要素, 荷重を受ける面積)
    for (const auto& element : surface_elements) {
        if (element.inside_fraction > 0.0) {
            loaded_elements.push_back({&element, element.area * element.inside_fraction});
        }
    }

    if (load.use_point && loaded_elements.empty() && !surface_elements.empty()) {
        // 半径がメッシュサイズより小さい場合は最も近い要素に荷重をかける
        const SurfaceElement* nearest = &surface_elements.front();
        for (const auto& element : surface_elements) {
            if (element.distance < nearest->distance) nearest = &element;
        }
        // 最も近い要素でも半径＋最大要素サイズより遠ければ、荷重点がこの面上にない（入力ミス）
        double max_element_size = 0.0;
        gmsh::option::getNumber("Mesh.CharacteristicLengthMax", max_element_size);
        if (nearest->distance > load.radius + max_element_size) {
            std::cerr << "エラー: 荷重点 (" << load.point[0] << ", " << load.point[1] << ", " << load.point[2]
                      << ") が Surface " << surface_number << " 上にありません（最も近い要素まで "
                      << nearest->distance << "）。" << std::endl;
            return false;
        }
        loaded_elements.push_back({nearest, nearest->area});
        std::cout << "警告: Surface " << surface_number << " の荷重点から半径 " << load.radius
                  << " 内に要素がありません。最も近い要素 (距離 " << nearest->distance << ") に荷重をかけます。" << std::endl;
        f << "** WARNING: no element within radius; using nearest element (distance "
          << nearest->distance << ")\n";
    }

    // ステップ3: 要素面積を節点に分配
    std::map<std::size_t, double> node_areas;  // 各節点の寄与面積
    double total_surface_area = 0.0;  // 荷重を受ける面積
    for (const auto& [element, loaded_area] : loaded_elements) {
        total_surface_area += loaded_area;
        double area_portion = loaded_area / element->node_tags.size();
        for (std::size_t node_tag : element->node_tags) {
            node_areas[node_tag] += area_portion;
        }
    }

    // ステップ4: 各節点にかかる力を計算
    if (total_surface_area > 0) {
        double pressure = total_force / total_surface_area;  // 荷重面の圧力

        // 力の方向ベクトルを正規化
        std::vector<double> normalized_direction = force_direction;
        double magnitude = std::sqrt(normalized_direction[0] * normalized_direction[0] +
                                   normalized_direction[1] * normalized_direction[1] +
                                   normalized_direction[2] * normalized_direction[2]);
        if (magnitude > 0) {
            for (double& component : normalized_direction) {
                component /= magnitude;
            }
        }

        f << "** Loaded elements: " << loaded_elements.size() << " / " << surface_elements.size() << "\n";
        f << "** Total surface area: " << std::fixed << std::setprecision(6) << total_surface_area << "\n";
        f << "** Pressure: " << std::fixed << std::setprecision(6) << pressure << " N/unit_area\n";

        // 各節点への力を計算・出力
        for (const auto& [node_tag, node_area] : node_areas) {
            double force_magnitude = pressure * node_area;
            std::vector<double> force_vector = {
                force_magnitude * normalized_direction[0],
                force_magnitude * normalized_direction[1],
                force_magnitude * normalized_direction[2]
            };

            // 各自由度に対する力成分を出力
            for (int dof = 1; dof <= 3; ++dof) {
                if (std::abs(force_vector[dof-1]) > 1e-12) {  // 微小な値は無視
                    f << node_tag << "," << dof << ","
                      << std::fixed << std::setprecision(6) << force_vector[dof-1] << "\n";
                }
            }
        }
    } else {
        std::cout << "警告: Surface " << surface_number << " の面積が0です。力の境界条件を適用できません。" << std::endl;
    }
    return true;
}

const std::vector<LoadProperties>& LoadConditionSetter::getLoads() const {
    return loads_;
}

LoadProperties createLoadCondition(int surface_number, double magnitude,
                                   const std::vector<double>& direction) {
    return {surface_number, magnitude, direction};
}

LoadProperties createPatchLoadCondition(int surface_number, double magnitude,
                                        const std::vector<double>& direction,
                                        const std::vector<double>& point, double radius) {
    LoadProperties load{surface_number, magnitude, direction};
    load.use_point = true;
    load.point = point;
    load.radius = radius;
    return load;
}
