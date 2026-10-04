#ifndef LOAD_CONDITION_SETTER_H
#define LOAD_CONDITION_SETTER_H

#include <vector>
#include <fstream>

struct LoadProperties {
    int surface_number;
    double magnitude;
    std::vector<double> direction;
    // Patch load: if use_point, only the part of the face within radius of point is loaded
    bool use_point = false;
    std::vector<double> point = {0.0, 0.0, 0.0};
    double radius = 0.0;
};

class LoadConditionSetter {
public:
    LoadConditionSetter();
    ~LoadConditionSetter();

    // Add load condition
    void addLoad(const LoadProperties& load);
    void addLoad(int surface_number, double magnitude, const std::vector<double>& direction);

    // Calculate element area (geometry utility)
    static double calculateElementArea(const std::vector<std::vector<double>>& coords);

    // Write load boundary conditions (whole face, or patch around load.point)
    // Returns false if a patch load's point is not on the surface
    bool writeForceBoundaryCondition(std::ofstream& f, const LoadProperties& load) const;

    // Get all load conditions
    const std::vector<LoadProperties>& getLoads() const;

private:
    std::vector<LoadProperties> loads_;
};

// Utility function
LoadProperties createLoadCondition(int surface_number, double magnitude,
                                   const std::vector<double>& direction);
LoadProperties createPatchLoadCondition(int surface_number, double magnitude,
                                        const std::vector<double>& direction,
                                        const std::vector<double>& point, double radius);

#endif // LOAD_CONDITION_SETTER_H
