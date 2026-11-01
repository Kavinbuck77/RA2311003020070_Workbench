#ifndef EMBEDDED_MOCK_HPP
#define EMBEDDED_MOCK_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <map>

namespace Workbench {

enum class DeviceType {
    MCU_STM32F4,      // Microcontroller: STM32F407 Cortex-M4 (UART/USB CDC @ 3.3V)
    FPGA_ARTIX7,      // FPGA Board: Xilinx Artix-7 (PCIe/SPI @ 1.8V/3.3V)
    JTAG_FT2232H      // Debug Adapter: FTDI FT2232H / J-Link (SWD/JTAG master @ 3.3V VREF)
};

enum class DeviceState {
    DISCONNECTED,
    DISCOVERING,
    CAPABILITY_QUERY,
    CONNECTED,
    STREAMING_TELEMETRY,
    ERROR_RECOVERY
};

enum class CommandId : uint8_t {
    CMD_DISCOVER       = 0x01,
    CMD_QUERY_CAPS     = 0x02,
    CMD_CONNECT        = 0x03,
    CMD_REQ_TELEMETRY  = 0x04,
    CMD_DISCONNECT     = 0x05,
    CMD_FAULT_INJECT   = 0x0E,
    CMD_FAULT_RECOVER  = 0x0F
};

struct PacketLog {
    uint32_t sequence_num = 0;
    uint8_t device_id = 0;
    std::string device_name;
    uint8_t cmd_id = 0;
    std::string cmd_name;
    std::string state_name;
    uint16_t payload_len = 0;
    std::string payload_hex;
    uint16_t expected_crc = 0;
    uint16_t received_crc = 0;
    bool crc_valid = true;
    bool is_retry = false;
    std::string status_message;
    double timestamp_ms = 0.0;
};

struct HardwareTelemetry {
    double rail_voltage_main = 0.0; // e.g. 3.30 V
    double rail_voltage_aux = 0.0;  // e.g. 1.80 V (FPGA) or 5.00 V (MCU)
    double rail_voltage_core = 0.0; // e.g. 1.00 V (FPGA VCCINT)
    double current_draw_mA = 0.0;   // e.g. 68.4 mA
    double die_temperature_C = 0.0; // e.g. 35.8 °C
    uint32_t uptime_ticks = 0;
    uint32_t packets_sent = 0;
    uint32_t packets_lost = 0;
    uint32_t recovered_faults = 0;
};

struct HardwareDescriptor {
    DeviceType type;
    std::string id_string;
    std::string display_name;
    std::string physical_bus;
    std::string logic_voltage_level;
    std::string clock_frequency;
    std::string capabilities_desc;
};

class EmbeddedDeviceAdapter {
public:
    EmbeddedDeviceAdapter(DeviceType type, uint8_t device_id);

    DeviceType get_type() const { return type_; }
    uint8_t get_device_id() const { return device_id_; }
    DeviceState get_state() const { return state_; }
    std::string get_state_string() const;
    const HardwareDescriptor& get_descriptor() const { return descriptor_; }
    const HardwareTelemetry& get_telemetry() const { return telemetry_; }

    // State machine operations
    PacketLog execute_discovery();
    PacketLog execute_capability_query();
    PacketLog execute_connect();
    PacketLog execute_telemetry_request(bool inject_crc_fault = false);
    PacketLog execute_fault_recovery_retry();
    PacketLog execute_disconnect();

    // Reset device state
    void reset();

    // Compute standard CCITT CRC-16 (Polynomial: 0x1021)
    static uint16_t compute_crc16(const uint8_t* data, size_t length);

private:
    DeviceType type_;
    uint8_t device_id_;
    DeviceState state_;
    HardwareDescriptor descriptor_;
    HardwareTelemetry telemetry_;
    uint32_t sequence_counter_;
    uint32_t command_count_;
    std::vector<uint8_t> last_corrupted_payload_;

    void update_mock_telemetry(bool step_tick = true);
};

class EmbeddedWorkbenchSubsystem {
public:
    EmbeddedWorkbenchSubsystem();

    void initialize();
    std::vector<PacketLog> execute_full_integration_sequence();
    PacketLog send_device_command(uint8_t device_id, CommandId cmd, bool inject_crc_error = false);
    void reset_all();

    const std::vector<PacketLog>& get_packet_history() const { return packet_history_; }
    const std::map<uint8_t, EmbeddedDeviceAdapter>& get_devices() const { return devices_; }

private:
    std::map<uint8_t, EmbeddedDeviceAdapter> devices_;
    std::vector<PacketLog> packet_history_;
    double subsystem_clock_ms_;
};

} // namespace Workbench

#endif // EMBEDDED_MOCK_HPP
