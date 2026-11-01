#ifndef CAD_ANSYS_PARSER_HPP
#define CAD_ANSYS_PARSER_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace Workbench {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct StlTriangle {
    Vec3 normal;
    Vec3 v1;
    Vec3 v2;
    Vec3 v3;
};

struct StlModel {
    std::string solid_name;
    std::vector<StlTriangle> triangles;
    Vec3 bbox_min;
    Vec3 bbox_max;
    double surface_area = 0.0;
    bool is_valid = false;
    std::string error_message;

    // Flattened geometry buffers for direct WebGL / Three.js BufferGeometry ingestion
    std::vector<float> position_buffer; // 9 floats per triangle (x1,y1,z1, x2,y2,z2, x3,y3,z3)
    std::vector<float> normal_buffer;   // 9 floats per triangle
};

struct AnsysNode {
    int node_id = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    double temperature = 0.0;
};

struct AnsysDataset {
    std::vector<AnsysNode> nodes;
    double min_temp = 0.0;
    double max_temp = 0.0;
    double mean_temp = 0.0;
    Vec3 bbox_min;
    Vec3 bbox_max;
    bool is_valid = false;
    std::string error_message;

    // Flattened buffers for Three.js point cloud & Plotly 3D scatter
    std::vector<float> point_coords; // 3 floats per point (x, y, z)
    std::vector<float> temperatures; // 1 float per point
};

// ASCII STL Parser
StlModel parse_ascii_stl_string(const std::string& stl_text);
StlModel parse_ascii_stl_file(const std::string& filepath);

// ANSYS CSV Parser
AnsysDataset parse_ansys_csv_string(const std::string& csv_text);
AnsysDataset parse_ansys_csv_file(const std::string& filepath);

// JSON Serialization helpers for REST API
std::string stl_model_to_json(const StlModel& model);
std::string ansys_dataset_to_json(const AnsysDataset& dataset);

// Generate realistic synthetic sample datasets
std::string generate_synthetic_stl();
std::string generate_synthetic_ansys_csv();

} // namespace Workbench

#endif // CAD_ANSYS_PARSER_HPP
