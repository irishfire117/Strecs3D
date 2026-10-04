#pragma once

#include <string>
#include <vector>
#include <array>
#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkPolyData.h>

class TopoDS_Shape;

// 面のジオメトリ情報を保持する構造体
struct FaceGeometry {
    double centerX, centerY, centerZ;  // 面の中心座標
    double normalX, normalY, normalZ;  // 面の法線ベクトル
    bool isValid;                       // 取得成功フラグ
};

// エッジのジオメトリ情報を保持する構造体
struct EdgeGeometry {
    double startX, startY, startZ;  // エッジの開始点座標
    double endX, endY, endZ;        // エッジの終了点座標
    double dirX, dirY, dirZ;        // 正規化された方向ベクトル
    bool isValid;                    // 取得成功フラグ
};

class StepReader {
public:
    StepReader();
    ~StepReader();

    // STEPファイルを読み込む
    bool readStepFile(const std::string& filename);

    // OpenCASCADE形状をVTKアクターに変換（面とエッジを保持）
    vtkSmartPointer<vtkActor> getFacesActor() const;
    vtkSmartPointer<vtkActor> getEdgesActor() const;

    // 形状が正常に読み込まれたかチェック
    bool isValid() const;

    // 面の総数を取得
    int getFaceCount() const { return faceCount_; }

    // 面ごとのアクターを取得（ホバー検出用）
    std::vector<vtkSmartPointer<vtkActor>> getFaceActors() const;
    std::vector<vtkSmartPointer<vtkActor>> getEdgeActors() const;

    // 面の中心と法線を取得（surface_idは1-based）
    FaceGeometry getFaceGeometry(int surfaceId) const;

    // 指定点を面に投影した点と、その点での法線を取得（center に投影点が入る）
    FaceGeometry getFaceGeometryAtPoint(int surfaceId, double x, double y, double z) const;

    // エッジのジオメトリを取得（edgeIdは1-based）
    EdgeGeometry getEdgeGeometry(int edgeId) const;

    // エッジ上の等間隔（パラメータ）サンプル点を取得（閉じたエッジも可、edgeIdは1-based）
    std::vector<std::array<double, 3>> getEdgeSamplePoints(int edgeId, int count) const;

    // エッジの総数
    int getEdgeCount() const;

    // ソリッドの総数（アセンブリなら複数）
    int getSolidCount() const;

private:
    TopoDS_Shape* shape_;
    bool isValid_;
    int faceCount_;

    // OpenCASCADE形状をVTKポリデータに変換
    vtkSmartPointer<vtkPolyData> convertFacesToPolyData() const;
    vtkSmartPointer<vtkPolyData> convertEdgesToPolyData() const;
};
