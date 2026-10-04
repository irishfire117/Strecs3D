#ifndef STEP2INP_H
#define STEP2INP_H

#include <string>
#include <vector>
#include <fstream>
#include "step2inp/MeshGenerator.h"
#include "step2inp/ConstraintSetter.h"
#include "step2inp/MaterialSetter.h"
#include "step2inp/LoadConditionSetter.h"
#include "step2inp/InpWriter.h"

// convert() / convertStepToInp() return codes
constexpr int STEP2INP_OK = 0;
constexpr int STEP2INP_ERROR = 1;
constexpr int STEP2INP_UNDER_CONSTRAINED = 2;    // constraints only fix one point or one line
constexpr int STEP2INP_NO_CONSTRAINED_NODES = 3; // a constraint's face/point/edge has no mesh nodes
constexpr int STEP2INP_POINT_OFF_FACE = 4;       // a patch load's point is not on its face
constexpr int STEP2INP_DISCONNECTED_SOLIDS = 5;  // several solids that share no faces (an assembly)

class Step2Inp {
public:
    Step2Inp();
    ~Step2Inp();

    // Main conversion method
    int convert(const std::string& step_file,
                const std::vector<ConstraintProperties>& constraints,
                const std::vector<LoadProperties>& loads,
                const std::string& output_file = "");

    // Access to components for advanced usage
    MeshGenerator& getMeshGenerator() { return mesh_generator_; }
    ConstraintSetter& getConstraintSetter() { return constraint_setter_; }
    MaterialSetter& getMaterialSetter() { return material_setter_; }
    LoadConditionSetter& getLoadConditionSetter() { return load_setter_; }
    InpWriter& getInpWriter() { return inp_writer_; }

private:
    // Component objects
    MeshGenerator mesh_generator_;
    ConstraintSetter constraint_setter_;
    MaterialSetter material_setter_;
    LoadConditionSetter load_setter_;
    InpWriter inp_writer_;
};

// Utility function
int convertStepToInp(const std::string& step_file,
                     const std::vector<ConstraintProperties>& constraints,
                     const std::vector<LoadProperties>& loads,
                     const std::string& output_file = "");

#endif // STEP2INP_H
