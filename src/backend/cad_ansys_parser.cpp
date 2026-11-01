#include "cad_ansys_parser.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <iomanip>
#include <limits>
#include <cctype>

namespace Workbench {

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static Vec3 cross_product(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static double vector_length(const Vec3& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

// -------------------------------------------------------------
// ASCII STL Parser
// -------------------------------------------------------------

StlModel parse_ascii_stl_string(const std::string& stl_text) {
    StlModel model;
    model.is_valid = false;
    model.bbox_min = { std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity() };
    model.bbox_max = { -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() };

    std::istringstream stream(stl_text);
    std::string line;

    // 1. First line must contain "solid"
    if (!std::getline(stream, line)) {
        model.error_message = "Empty STL content.";
        return model;
    }
    std::string trimmed_first = trim(line);
    if (trimmed_first.rfind("solid", 0) != 0) {
        model.error_message = "STL format error: Expected keyword 'solid' at start of file.";
        return model;
    }
    model.solid_name = trim(trimmed_first.substr(5));
    if (model.solid_name.empty()) model.solid_name = "unnamed_solid";

    bool inside_facet = false;
    bool inside_loop = false;
    StlTriangle current_triangle;
    std::vector<Vec3> current_vertices;

    while (std::getline(stream, line)) {
        std::string tline = trim(line);
        if (tline.empty()) continue;

        std::istringstream line_stream(tline);
        std::string token;
        line_stream >> token;

        if (token == "facet") {
            std::string sub_token;
            line_stream >> sub_token;
            if (sub_token != "normal") {
                model.error_message = "STL format error: Expected 'normal' after 'facet'. Found: " + sub_token;
                return model;
            }
            double nx, ny, nz;
            if (!(line_stream >> nx >> ny >> nz) || !std::isfinite(nx) || !std::isfinite(ny) || !std::isfinite(nz)) {
                model.error_message = "STL format error: Non-finite normal vector floating point values.";
                return model;
            }
            current_triangle.normal = {nx, ny, nz};
            inside_facet = true;
            current_vertices.clear();
        } else if (token == "outer") {
            std::string sub_token;
            line_stream >> sub_token;
            if (sub_token != "loop" || !inside_facet) {
                model.error_message = "STL format error: Unexpected 'outer " + sub_token + "' outside facet.";
                return model;
            }
            inside_loop = true;
        } else if (token == "vertex") {
            if (!inside_loop) {
                model.error_message = "STL format error: 'vertex' declared outside 'outer loop'.";
                return model;
            }
            double vx, vy, vz;
            if (!(line_stream >> vx >> vy >> vz) || !std::isfinite(vx) || !std::isfinite(vy) || !std::isfinite(vz)) {
                model.error_message = "STL format error: Non-finite vertex coordinate floating point values.";
                return model;
            }
            current_vertices.push_back({vx, vy, vz});
        } else if (token == "endloop") {
            if (!inside_loop || current_vertices.size() != 3) {
                model.error_message = "STL format error: 'endloop' encountered but triangle vertex count is " + std::to_string(current_vertices.size()) + " (expected 3).";
                return model;
            }
            inside_loop = false;
            current_triangle.v1 = current_vertices[0];
            current_triangle.v2 = current_vertices[1];
            current_triangle.v3 = current_vertices[2];
        } else if (token == "endfacet") {
            if (!inside_facet) {
                model.error_message = "STL format error: Unmatched 'endfacet'.";
                return model;
            }
            inside_facet = false;

            // Geometry bounding box update
            for (const auto& v : {current_triangle.v1, current_triangle.v2, current_triangle.v3}) {
                model.bbox_min.x = std::min(model.bbox_min.x, v.x);
                model.bbox_min.y = std::min(model.bbox_min.y, v.y);
                model.bbox_min.z = std::min(model.bbox_min.z, v.z);
                model.bbox_max.x = std::max(model.bbox_max.x, v.x);
                model.bbox_max.y = std::max(model.bbox_max.y, v.y);
                model.bbox_max.z = std::max(model.bbox_max.z, v.z);
            }

            // Triangle surface area: 0.5 * ||(v2 - v1) x (v3 - v1)||
            Vec3 edge1 = { current_triangle.v2.x - current_triangle.v1.x, current_triangle.v2.y - current_triangle.v1.y, current_triangle.v2.z - current_triangle.v1.z };
            Vec3 edge2 = { current_triangle.v3.x - current_triangle.v1.x, current_triangle.v3.y - current_triangle.v1.y, current_triangle.v3.z - current_triangle.v1.z };
            Vec3 normal_calc = cross_product(edge1, edge2);
            double tri_area = 0.5 * vector_length(normal_calc);
            model.surface_area += tri_area;

            // Re-normalize normal if needed
            double n_len = vector_length(normal_calc);
            if (n_len > 1.0e-12 && vector_length(current_triangle.normal) < 1.0e-6) {
                current_triangle.normal = { normal_calc.x / n_len, normal_calc.y / n_len, normal_calc.z / n_len };
            }

            // Append to linear buffers for Three.js
            for (const auto& v : {current_triangle.v1, current_triangle.v2, current_triangle.v3}) {
                model.position_buffer.push_back(static_cast<float>(v.x));
                model.position_buffer.push_back(static_cast<float>(v.y));
                model.position_buffer.push_back(static_cast<float>(v.z));

                model.normal_buffer.push_back(static_cast<float>(current_triangle.normal.x));
                model.normal_buffer.push_back(static_cast<float>(current_triangle.normal.y));
                model.normal_buffer.push_back(static_cast<float>(current_triangle.normal.z));
            }

            model.triangles.push_back(current_triangle);
        } else if (token == "endsolid") {
            break;
        }
    }

    if (model.triangles.empty()) {
        model.error_message = "STL parsing failed: Zero valid facets parsed.";
        return model;
    }

    model.is_valid = true;
    return model;
}

StlModel parse_ascii_stl_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        StlModel m;
        m.error_message = "Could not open STL file: " + filepath;
        return m;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_ascii_stl_string(buffer.str());
}

// -------------------------------------------------------------
// ANSYS CSV Parser
// -------------------------------------------------------------

AnsysDataset parse_ansys_csv_string(const std::string& csv_text) {
    AnsysDataset dataset;
    dataset.is_valid = false;
    dataset.min_temp = std::numeric_limits<double>::infinity();
    dataset.max_temp = -std::numeric_limits<double>::infinity();
    dataset.mean_temp = 0.0;
    dataset.bbox_min = { std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity() };
    dataset.bbox_max = { -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity() };

    std::istringstream stream(csv_text);
    std::string line;

    // Header validation
    if (!std::getline(stream, line)) {
        dataset.error_message = "Empty ANSYS CSV file.";
        return dataset;
    }

    std::string header = trim(line);
    // Remove potential UTF-8 BOM
    if (header.size() >= 3 && static_cast<unsigned char>(header[0]) == 0xEF &&
        static_cast<unsigned char>(header[1]) == 0xBB && static_cast<unsigned char>(header[2]) == 0xBF) {
        header = header.substr(3);
    }

    // Required columns check: node_id,x_m,y_m,z_m,temperature_C
    if (header.find("node_id") == std::string::npos ||
        header.find("x_m") == std::string::npos ||
        header.find("y_m") == std::string::npos ||
        header.find("z_m") == std::string::npos ||
        header.find("temperature_C") == std::string::npos) {
        dataset.error_message = "ANSYS CSV Header mismatch. Expected columns: node_id,x_m,y_m,z_m,temperature_C. Found: " + header;
        return dataset;
    }

    std::unordered_set<int> seen_node_ids;
    double temp_sum = 0.0;
    int line_num = 1;

    while (std::getline(stream, line)) {
        line_num++;
        std::string tline = trim(line);
        if (tline.empty()) continue;

        std::stringstream ss(tline);
        std::string item;
        std::vector<std::string> fields;

        while (std::getline(ss, item, ',')) {
            fields.push_back(trim(item));
        }

        if (fields.size() != 5) {
            dataset.error_message = "Line " + std::to_string(line_num) + ": Expected 5 comma-separated values, found " + std::to_string(fields.size());
            return dataset;
        }

        int node_id = 0;
        double x = 0.0, y = 0.0, z = 0.0, temp = 0.0;

        try {
            node_id = std::stoi(fields[0]);
            x = std::stod(fields[1]);
            y = std::stod(fields[2]);
            z = std::stod(fields[3]);
            temp = std::stod(fields[4]);
        } catch (...) {
            dataset.error_message = "Line " + std::to_string(line_num) + ": Numeric conversion error (malformed float/int).";
            return dataset;
        }

        // Validate finiteness (reject NaN, Inf)
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(temp)) {
            dataset.error_message = "Line " + std::to_string(line_num) + ": Non-finite numerical value detected (NaN or Inf).";
            return dataset;
        }

        // Validate uniqueness of node_id
        if (seen_node_ids.find(node_id) != seen_node_ids.end()) {
            dataset.error_message = "Line " + std::to_string(line_num) + ": Duplicate node_id detected: " + std::to_string(node_id);
            return dataset;
        }
        seen_node_ids.insert(node_id);

        AnsysNode node;
        node.node_id = node_id;
        node.x = x;
        node.y = y;
        node.z = z;
        node.temperature = temp;

        dataset.nodes.push_back(node);
        temp_sum += temp;

        dataset.min_temp = std::min(dataset.min_temp, temp);
        dataset.max_temp = std::max(dataset.max_temp, temp);

        dataset.bbox_min.x = std::min(dataset.bbox_min.x, x);
        dataset.bbox_min.y = std::min(dataset.bbox_min.y, y);
        dataset.bbox_min.z = std::min(dataset.bbox_min.z, z);
        dataset.bbox_max.x = std::max(dataset.bbox_max.x, x);
        dataset.bbox_max.y = std::max(dataset.bbox_max.y, y);
        dataset.bbox_max.z = std::max(dataset.bbox_max.z, z);

        dataset.point_coords.push_back(static_cast<float>(x));
        dataset.point_coords.push_back(static_cast<float>(y));
        dataset.point_coords.push_back(static_cast<float>(z));
        dataset.temperatures.push_back(static_cast<float>(temp));
    }

    if (dataset.nodes.empty()) {
        dataset.error_message = "ANSYS CSV parsing failed: Zero data rows parsed.";
        return dataset;
    }

    dataset.mean_temp = temp_sum / static_cast<double>(dataset.nodes.size());
    dataset.is_valid = true;
    return dataset;
}

AnsysDataset parse_ansys_csv_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        AnsysDataset d;
        d.error_message = "Could not open ANSYS CSV file: " + filepath;
        return d;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_ansys_csv_string(buffer.str());
}

// -------------------------------------------------------------
// JSON Serialization
// -------------------------------------------------------------

std::string stl_model_to_json(const StlModel& model) {
    std::ostringstream ss;
    ss << "{\"valid\":" << (model.is_valid ? "true" : "false");
    if (!model.is_valid) {
        ss << ",\"error\":\"" << model.error_message << "\"}";
        return ss.str();
    }

    ss << ",\"name\":\"" << model.solid_name << "\"";
    ss << ",\"triangle_count\":" << model.triangles.size();
    ss << ",\"surface_area\":" << model.surface_area;
    ss << ",\"bbox_min\":[" << model.bbox_min.x << "," << model.bbox_min.y << "," << model.bbox_min.z << "]";
    ss << ",\"bbox_max\":[" << model.bbox_max.x << "," << model.bbox_max.y << "," << model.bbox_max.z << "]";

    // Output positions array
    ss << ",\"positions\":[";
    for (size_t i = 0; i < model.position_buffer.size(); ++i) {
        ss << model.position_buffer[i];
        if (i + 1 < model.position_buffer.size()) ss << ",";
    }
    ss << "]";

    // Output normals array
    ss << ",\"normals\":[";
    for (size_t i = 0; i < model.normal_buffer.size(); ++i) {
        ss << model.normal_buffer[i];
        if (i + 1 < model.normal_buffer.size()) ss << ",";
    }
    ss << "]}";

    return ss.str();
}

std::string ansys_dataset_to_json(const AnsysDataset& dataset) {
    std::ostringstream ss;
    ss << "{\"valid\":" << (dataset.is_valid ? "true" : "false");
    if (!dataset.is_valid) {
        ss << ",\"error\":\"" << dataset.error_message << "\"}";
        return ss.str();
    }

    ss << ",\"node_count\":" << dataset.nodes.size();
    ss << ",\"min_temp\":" << dataset.min_temp;
    ss << ",\"max_temp\":" << dataset.max_temp;
    ss << ",\"mean_temp\":" << dataset.mean_temp;
    ss << ",\"bbox_min\":[" << dataset.bbox_min.x << "," << dataset.bbox_min.y << "," << dataset.bbox_min.z << "]";
    ss << ",\"bbox_max\":[" << dataset.bbox_max.x << "," << dataset.bbox_max.y << "," << dataset.bbox_max.z << "]";

    // Output points
    ss << ",\"points\":[";
    for (size_t i = 0; i < dataset.point_coords.size(); ++i) {
        ss << dataset.point_coords[i];
        if (i + 1 < dataset.point_coords.size()) ss << ",";
    }
    ss << "]";

    // Output temperatures
    ss << ",\"temperatures\":[";
    for (size_t i = 0; i < dataset.temperatures.size(); ++i) {
        ss << dataset.temperatures[i];
        if (i + 1 < dataset.temperatures.size()) ss << ",";
    }
    ss << "]}";

    return ss.str();
}

// -------------------------------------------------------------
// Synthetic Sample Generators
// -------------------------------------------------------------

static void emit_box_facets(std::ostringstream& ss, double x0, double y0, double z0, double x1, double y1, double z1) {
    // 6 faces * 2 triangles = 12 triangles per box
    auto emit_tri = [&](double nx, double ny, double nz,
                        double ax, double ay, double az,
                        double bx, double by, double bz,
                        double cx, double cy, double cz) {
        ss << "  facet normal " << nx << " " << ny << " " << nz << "\n"
           << "    outer loop\n"
           << "      vertex " << ax << " " << ay << " " << az << "\n"
           << "      vertex " << bx << " " << by << " " << bz << "\n"
           << "      vertex " << cx << " " << cy << " " << cz << "\n"
           << "    endloop\n"
           << "  endfacet\n";
    };

    // Bottom face (-Z)
    emit_tri(0, 0, -1,  x0, y0, z0,   x1, y0, z0,   x1, y1, z0);
    emit_tri(0, 0, -1,  x0, y0, z0,   x1, y1, z0,   x0, y1, z0);
    // Top face (+Z)
    emit_tri(0, 0, 1,   x0, y0, z1,   x1, y1, z1,   x1, y0, z1);
    emit_tri(0, 0, 1,   x0, y0, z1,   x0, y1, z1,   x1, y1, z1);
    // Front face (-Y)
    emit_tri(0, -1, 0,  x0, y0, z0,   x1, y0, z1,   x1, y0, z0);
    emit_tri(0, -1, 0,  x0, y0, z0,   x0, y0, z1,   x1, y0, z1);
    // Back face (+Y)
    emit_tri(0, 1, 0,   x0, y1, z0,   x1, y1, z0,   x1, y1, z1);
    emit_tri(0, 1, 0,   x0, y1, z0,   x1, y1, z1,   x0, y1, z1);
    // Left face (-X)
    emit_tri(-1, 0, 0,  x0, y0, z0,   x0, y1, z0,   x0, y1, z1);
    emit_tri(-1, 0, 0,  x0, y0, z0,   x0, y1, z1,   x0, y0, z1);
    // Right face (+X)
    emit_tri(1, 0, 0,   x1, y0, z0,   x1, y0, z1,   x1, y1, z1);
    emit_tri(1, 0, 0,   x1, y0, z0,   x1, y1, z1,   x1, y1, z0);
}

std::string generate_synthetic_stl() {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4);
    ss << "solid HeatSinkBracket\n";

    // Base plate: [0, 0.10] x [0, 0.05] x [0, 0.008]
    emit_box_facets(ss, 0.0, 0.0, 0.0, 0.10, 0.05, 0.008);

    // Fin 1: [0.015, 0.025] x [0.005, 0.045] x [0.008, 0.035]
    emit_box_facets(ss, 0.015, 0.005, 0.008, 0.025, 0.045, 0.035);

    // Fin 2: [0.045, 0.055] x [0.005, 0.045] x [0.008, 0.035]
    emit_box_facets(ss, 0.045, 0.005, 0.008, 0.055, 0.045, 0.035);

    // Fin 3: [0.075, 0.085] x [0.005, 0.045] x [0.008, 0.035]
    emit_box_facets(ss, 0.075, 0.005, 0.008, 0.085, 0.045, 0.035);

    ss << "endsolid HeatSinkBracket\n";
    return ss.str();
}

std::string generate_synthetic_ansys_csv() {
    std::ostringstream ss;
    ss << "node_id,x_m,y_m,z_m,temperature_C\n";
    ss << std::fixed << std::setprecision(5);

    int node_counter = 1001;

    // Generate 3D grid of nodes on the heat sink geometry
    // Base plate nodes: Nx = 11, Ny = 6, Nz = 3
    for (int ix = 0; ix <= 10; ++ix) {
        double x = ix * 0.01;
        for (int iy = 0; iy <= 5; ++iy) {
            double y = iy * 0.01;
            for (int iz = 0; iz <= 2; ++iz) {
                double z = iz * 0.004;
                // Realistic heat conduction: Hot at base contact (z=0, center), cooling along Z and edges
                double dist_center = std::sqrt(std::pow(x - 0.05, 2) + std::pow(y - 0.025, 2));
                double temp = 95.0 - 45.0 * (z / 0.008) - 15.0 * (dist_center / 0.05);
                if (temp < 25.0) temp = 25.0;
                ss << node_counter++ << "," << x << "," << y << "," << z << "," << temp << "\n";
            }
        }
    }

    // Cooling Fin nodes: 3 fins, each has fine resolution along Z
    double fin_x_centers[3] = { 0.020, 0.050, 0.080 };
    for (int f = 0; f < 3; ++f) {
        double fx = fin_x_centers[f];
        for (int iy = 0; iy <= 4; ++iy) {
            double fy = 0.005 + iy * 0.01;
            for (int iz = 1; iz <= 6; ++iz) {
                double fz = 0.008 + iz * 0.0045; // up to ~0.035 m
                // Fin convective cooling profile: T(z) = T_ambient + (T_base - T_ambient) * cosh(m*(L-z))/cosh(m*L)
                double z_ratio = (fz - 0.008) / (0.035 - 0.008);
                double t_base = 75.0 - std::abs(fx - 0.050) * 100.0;
                double temp = 22.0 + (t_base - 22.0) * std::exp(-2.2 * z_ratio);
                ss << node_counter++ << "," << fx << "," << fy << "," << fz << "," << temp << "\n";
            }
        }
    }

    return ss.str();
}

} // namespace Workbench
