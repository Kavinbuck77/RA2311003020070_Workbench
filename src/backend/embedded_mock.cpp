#include "embedded_mock.hpp"
#include <sstream>
#include <iomanip>
#include <chrono>

namespace Workbench {

uint16_t EmbeddedDeviceAdapter::compute_crc16(const uint8_t* data, size_t length) {
    // Standard CRC-16-CCITT (Polynomial 0x1021, Initial 0xFFFF)
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = (crc << 1);
            }
        }
    }
    return crc;
}

static std::string bytes_to_hex(const uint8_t* data, size_t length) {
    std::ostringstream ss;
    for (size_t i = 0; i < length; ++i) {
        ss << std::hex << std::uppercase << std::setfill('0') << std::setw(2) << static_cast<int>(data[i]);
        if (i + 1 < length) ss << " ";
    }
    return ss.str();
}

EmbeddedDeviceAdapter::EmbeddedDeviceAdapter(DeviceType type, uint8_t device_id)
    : type_(type), device_id_(device_id), state_(DeviceState::DISCONNECTED), sequence_counter_(0), command_count_(0) {
    reset();
}

void EmbeddedDeviceAdapter::reset() {
    state_ = DeviceState::DISCONNECTED;
    sequence_counter_ = 100;
    command_count_ = 0;
    last_corrupted_payload_.clear();

    telemetry_.uptime_ticks = 0;
    telemetry_.packets_sent = 0;
    telemetry_.packets_lost = 0;
    telemetry_.recovered_faults = 0;

    switch (type_) {
        case DeviceType::MCU_STM32F4:
            descriptor_.type = type_;
            descriptor_.id_string = "STM32F407VGT6";
            descriptor_.display_name = "STM32F407 High-Performance ARM Cortex-M4 MCU";
            descriptor_.physical_bus = "USB 2.0 CDC / UART @ 115200 8N1";
            descriptor_.logic_voltage_level = "3.3V CMOS Logic (5V tolerant)";
            descriptor_.clock_frequency = "168 MHz PLL / 16 MHz HSI";
            descriptor_.capabilities_desc = "Single-precision HW FPU, 192KB SRAM, 1MB Flash, 12-bit ADC, Dual DMA";
            telemetry_.rail_voltage_main = 3.312;
            telemetry_.rail_voltage_aux = 5.021;
            telemetry_.rail_voltage_core = 3.312;
            telemetry_.current_draw_mA = 68.4;
            telemetry_.die_temperature_C = 34.8;
            break;

        case DeviceType::FPGA_ARTIX7:
            descriptor_.type = type_;
            descriptor_.id_string = "XC7A100T-1CSG324C";
            descriptor_.display_name = "Xilinx Artix-7 Reconfigurable Fabric";
            descriptor_.physical_bus = "PCIe Gen2 x1 / SPI Master @ 50 MHz";
            descriptor_.logic_voltage_level = "Core VCCINT: 1.0V | Aux VCCAUX: 1.8V | IO VCCO: 3.3V";
            descriptor_.clock_frequency = "100 MHz RefClk / 450 MHz Fabric";
            descriptor_.capabilities_desc = "101,440 Logic Cells, 240 DSP48E1 slices, 4,860 Kb BRAM, Dual 12-bit XADC";
            telemetry_.rail_voltage_main = 3.305;
            telemetry_.rail_voltage_aux = 1.802;
            telemetry_.rail_voltage_core = 1.004;
            telemetry_.current_draw_mA = 485.2;
            telemetry_.die_temperature_C = 43.6;
            break;

        case DeviceType::JTAG_FT2232H:
            descriptor_.type = type_;
            descriptor_.id_string = "FT2232H-JLINK";
            descriptor_.display_name = "FTDI FT2232H / J-Link Debug Interface";
            descriptor_.physical_bus = "USB 2.0 High-Speed (480 Mbps) to ARM 20-Pin SWD";
            descriptor_.logic_voltage_level = "Target VREF Sense: 1.65V - 3.60V (Active 3.30V)";
            descriptor_.clock_frequency = "Adaptive SWD Clock: 4.0 MHz";
            descriptor_.capabilities_desc = "Hardware SWD/JTAG Master, SWV ITM Trace Snooper, Hardware Reset Line Drive";
            telemetry_.rail_voltage_main = 3.298;
            telemetry_.rail_voltage_aux = 5.008;
            telemetry_.rail_voltage_core = 3.298;
            telemetry_.current_draw_mA = 32.1;
            telemetry_.die_temperature_C = 28.5;
            break;
    }
}

std::string EmbeddedDeviceAdapter::get_state_string() const {
    switch (state_) {
        case DeviceState::DISCONNECTED: return "DISCONNECTED";
        case DeviceState::DISCOVERING: return "DISCOVERING";
        case DeviceState::CAPABILITY_QUERY: return "CAPABILITY_QUERY";
        case DeviceState::CONNECTED: return "CONNECTED";
        case DeviceState::STREAMING_TELEMETRY: return "STREAMING_TELEMETRY";
        case DeviceState::ERROR_RECOVERY: return "ERROR_RECOVERY";
    }
    return "UNKNOWN";
}

void EmbeddedDeviceAdapter::update_mock_telemetry(bool step_tick) {
    if (step_tick) {
        telemetry_.uptime_ticks += 100;
        telemetry_.packets_sent += 1;
    }
    // Small realistic micro-variations
    switch (type_) {
        case DeviceType::MCU_STM32F4:
            telemetry_.current_draw_mA = 68.0 + (telemetry_.uptime_ticks % 15) * 0.1;
            telemetry_.die_temperature_C = 34.5 + (telemetry_.uptime_ticks % 20) * 0.05;
            break;
        case DeviceType::FPGA_ARTIX7:
            telemetry_.current_draw_mA = 480.0 + (telemetry_.uptime_ticks % 50) * 0.2;
            telemetry_.die_temperature_C = 43.0 + (telemetry_.uptime_ticks % 30) * 0.08;
            break;
        case DeviceType::JTAG_FT2232H:
            telemetry_.current_draw_mA = 32.0 + (telemetry_.uptime_ticks % 10) * 0.04;
            telemetry_.die_temperature_C = 28.2 + (telemetry_.uptime_ticks % 12) * 0.03;
            break;
    }
}

PacketLog EmbeddedDeviceAdapter::execute_discovery() {
    state_ = DeviceState::DISCOVERING;
    command_count_++;
    sequence_counter_++;
    update_mock_telemetry();

    std::vector<uint8_t> payload = {'P', 'I', 'N', 'G', 0x01, device_id_};
    uint16_t crc = compute_crc16(payload.data(), payload.size());

    PacketLog log;
    log.sequence_num = sequence_counter_;
    log.device_id = device_id_;
    log.device_name = descriptor_.id_string;
    log.cmd_id = static_cast<uint8_t>(CommandId::CMD_DISCOVER);
    log.cmd_name = "CMD_DISCOVER";
    log.state_name = get_state_string();
    log.payload_len = static_cast<uint16_t>(payload.size());
    log.payload_hex = bytes_to_hex(payload.data(), payload.size());
    log.expected_crc = crc;
    log.received_crc = crc;
    log.crc_valid = true;
    log.is_retry = false;
    log.status_message = "Handshake SYN-ACK received: device enumerated on " + descriptor_.physical_bus;

    state_ = DeviceState::CAPABILITY_QUERY;
    return log;
}

PacketLog EmbeddedDeviceAdapter::execute_capability_query() {
    state_ = DeviceState::CAPABILITY_QUERY;
    command_count_++;
    sequence_counter_++;
    update_mock_telemetry();

    std::string caps_str = descriptor_.id_string + ";" + descriptor_.logic_voltage_level;
    std::vector<uint8_t> payload(caps_str.begin(), caps_str.end());
    uint16_t crc = compute_crc16(payload.data(), payload.size());

    PacketLog log;
    log.sequence_num = sequence_counter_;
    log.device_id = device_id_;
    log.device_name = descriptor_.id_string;
    log.cmd_id = static_cast<uint8_t>(CommandId::CMD_QUERY_CAPS);
    log.cmd_name = "CMD_QUERY_CAPS";
    log.state_name = get_state_string();
    log.payload_len = static_cast<uint16_t>(payload.size());
    log.payload_hex = bytes_to_hex(payload.data(), payload.size());
    log.expected_crc = crc;
    log.received_crc = crc;
    log.crc_valid = true;
    log.is_retry = false;
    log.status_message = "Capabilities acknowledged: " + descriptor_.capabilities_desc;

    return log;
}

PacketLog EmbeddedDeviceAdapter::execute_connect() {
    command_count_++;
    sequence_counter_++;
    update_mock_telemetry();

    std::vector<uint8_t> payload = {0xC0, 0x11, 0xEC, 0x70, device_id_, 0x01};
    uint16_t crc = compute_crc16(payload.data(), payload.size());

    PacketLog log;
    log.sequence_num = sequence_counter_;
    log.device_id = device_id_;
    log.device_name = descriptor_.id_string;
    log.cmd_id = static_cast<uint8_t>(CommandId::CMD_CONNECT);
    log.cmd_name = "CMD_CONNECT";
    log.payload_len = static_cast<uint16_t>(payload.size());
    log.payload_hex = bytes_to_hex(payload.data(), payload.size());
    log.expected_crc = crc;
    log.received_crc = crc;
    log.crc_valid = true;
    log.is_retry = false;
    log.status_message = "Session established: baud/clock synchronized at " + descriptor_.clock_frequency;

    state_ = DeviceState::STREAMING_TELEMETRY;
    log.state_name = get_state_string();
    return log;
}

PacketLog EmbeddedDeviceAdapter::execute_telemetry_request(bool inject_crc_fault) {
    command_count_++;
    sequence_counter_++;
    update_mock_telemetry();

    // Binary telemetry payload: [Vmain_mV_u16, I_mA_u16, T_cdegC_u16, ticks_u32]
    uint16_t v_main_mv = static_cast<uint16_t>(telemetry_.rail_voltage_main * 1000.0);
    uint16_t current_ma = static_cast<uint16_t>(telemetry_.current_draw_mA);
    uint16_t temp_cdeg = static_cast<uint16_t>(telemetry_.die_temperature_C * 100.0);
    uint32_t ticks = telemetry_.uptime_ticks;

    std::vector<uint8_t> payload = {
        static_cast<uint8_t>(v_main_mv >> 8), static_cast<uint8_t>(v_main_mv & 0xFF),
        static_cast<uint8_t>(current_ma >> 8), static_cast<uint8_t>(current_ma & 0xFF),
        static_cast<uint8_t>(temp_cdeg >> 8), static_cast<uint8_t>(temp_cdeg & 0xFF),
        static_cast<uint8_t>((ticks >> 24) & 0xFF), static_cast<uint8_t>((ticks >> 16) & 0xFF),
        static_cast<uint8_t>((ticks >> 8) & 0xFF), static_cast<uint8_t>(ticks & 0xFF)
    };

    uint16_t computed_crc = compute_crc16(payload.data(), payload.size());
    uint16_t transmitted_crc = computed_crc;

    PacketLog log;
    log.sequence_num = sequence_counter_;
    log.device_id = device_id_;
    log.device_name = descriptor_.id_string;
    log.cmd_id = static_cast<uint8_t>(CommandId::CMD_REQ_TELEMETRY);
    log.cmd_name = "CMD_REQ_TELEMETRY";
    log.payload_len = static_cast<uint16_t>(payload.size());
    log.expected_crc = computed_crc;
    log.is_retry = false;

    if (inject_crc_fault) {
        // Corrupt transmitted CRC (simulate transient bit-flip / parity failure)
        transmitted_crc ^= 0xDEAD;
        log.received_crc = transmitted_crc;
        log.crc_valid = false;
        state_ = DeviceState::ERROR_RECOVERY;
        telemetry_.packets_lost++;
        last_corrupted_payload_ = payload;

        log.state_name = get_state_string();
        log.payload_hex = bytes_to_hex(payload.data(), payload.size());
        log.status_message = "[FAULT INJECTED] CRC16 mismatch: expected 0x" +
            [&]() { std::ostringstream s; s << std::hex << std::uppercase << computed_crc; return s.str(); }() +
            ", got 0x" +
            [&]() { std::ostringstream s; s << std::hex << std::uppercase << transmitted_crc; return s.str(); }() +
            " -> Transitioned to ERROR_RECOVERY";
    } else {
        log.received_crc = transmitted_crc;
        log.crc_valid = true;
        log.state_name = get_state_string();
        log.payload_hex = bytes_to_hex(payload.data(), payload.size());
        log.status_message = "Telemetry packet verified OK. Vmain=" + std::to_string(telemetry_.rail_voltage_main) +
                             "V, Temp=" + std::to_string(telemetry_.die_temperature_C) + "C";
    }

    return log;
}

PacketLog EmbeddedDeviceAdapter::execute_fault_recovery_retry() {
    command_count_++;
    sequence_counter_++;
    update_mock_telemetry();

    std::vector<uint8_t> payload = last_corrupted_payload_;
    if (payload.empty()) {
        payload = {0xAA, 0xBB, 0xCC, 0xDD};
    }
    uint16_t clean_crc = compute_crc16(payload.data(), payload.size());

    PacketLog log;
    log.sequence_num = sequence_counter_;
    log.device_id = device_id_;
    log.device_name = descriptor_.id_string;
    log.cmd_id = static_cast<uint8_t>(CommandId::CMD_FAULT_RECOVER);
    log.cmd_name = "CMD_RETRANSMIT_RECOVER";
    log.payload_len = static_cast<uint16_t>(payload.size());
    log.payload_hex = bytes_to_hex(payload.data(), payload.size());
    log.expected_crc = clean_crc;
    log.received_crc = clean_crc;
    log.crc_valid = true;
    log.is_retry = true;

    telemetry_.recovered_faults++;
    state_ = DeviceState::STREAMING_TELEMETRY;
    log.state_name = get_state_string();
    log.status_message = "[FAULT RESOLVED] Retransmitted packet acknowledged. CRC16 valid (0x" +
        [&]() { std::ostringstream s; s << std::hex << std::uppercase << clean_crc; return s.str(); }() +
        ") -> Returned to STREAMING_TELEMETRY";

    return log;
}

PacketLog EmbeddedDeviceAdapter::execute_disconnect() {
    command_count_++;
    sequence_counter_++;
    update_mock_telemetry(false);

    std::vector<uint8_t> payload = {0xD1, 0x5C, 0x00, device_id_};
    uint16_t crc = compute_crc16(payload.data(), payload.size());

    PacketLog log;
    log.sequence_num = sequence_counter_;
    log.device_id = device_id_;
    log.device_name = descriptor_.id_string;
    log.cmd_id = static_cast<uint8_t>(CommandId::CMD_DISCONNECT);
    log.cmd_name = "CMD_DISCONNECT";
    log.payload_len = static_cast<uint16_t>(payload.size());
    log.payload_hex = bytes_to_hex(payload.data(), payload.size());
    log.expected_crc = crc;
    log.received_crc = crc;
    log.crc_valid = true;
    log.is_retry = false;

    state_ = DeviceState::DISCONNECTED;
    log.state_name = get_state_string();
    log.status_message = "Device gracefully released. Bus tri-stated.";
    return log;
}

// -------------------------------------------------------------
// EmbeddedWorkbenchSubsystem Implementation
// -------------------------------------------------------------

EmbeddedWorkbenchSubsystem::EmbeddedWorkbenchSubsystem() : subsystem_clock_ms_(0.0) {
    initialize();
}

void EmbeddedWorkbenchSubsystem::initialize() {
    devices_.clear();
    packet_history_.clear();
    subsystem_clock_ms_ = 0.0;

    devices_.emplace(0x10, EmbeddedDeviceAdapter(DeviceType::MCU_STM32F4, 0x10));
    devices_.emplace(0x20, EmbeddedDeviceAdapter(DeviceType::FPGA_ARTIX7, 0x20));
    devices_.emplace(0x30, EmbeddedDeviceAdapter(DeviceType::JTAG_FT2232H, 0x30));
}

void EmbeddedWorkbenchSubsystem::reset_all() {
    initialize();
}

PacketLog EmbeddedWorkbenchSubsystem::send_device_command(uint8_t device_id, CommandId cmd, bool inject_crc_error) {
    subsystem_clock_ms_ += 12.5; // Simulated bus clock delta
    auto it = devices_.find(device_id);
    if (it == devices_.end()) {
        PacketLog err;
        err.device_id = device_id;
        err.status_message = "Target device ID not found in subsystem enumeration table";
        return err;
    }

    PacketLog log;
    switch (cmd) {
        case CommandId::CMD_DISCOVER:
            log = it->second.execute_discovery();
            break;
        case CommandId::CMD_QUERY_CAPS:
            log = it->second.execute_capability_query();
            break;
        case CommandId::CMD_CONNECT:
            log = it->second.execute_connect();
            break;
        case CommandId::CMD_REQ_TELEMETRY:
            log = it->second.execute_telemetry_request(inject_crc_error);
            break;
        case CommandId::CMD_FAULT_RECOVER:
            log = it->second.execute_fault_recovery_retry();
            break;
        case CommandId::CMD_DISCONNECT:
            log = it->second.execute_disconnect();
            break;
        default:
            log.status_message = "Unsupported command opcode";
            break;
    }
    log.timestamp_ms = subsystem_clock_ms_;
    packet_history_.push_back(log);
    return log;
}

std::vector<PacketLog> EmbeddedWorkbenchSubsystem::execute_full_integration_sequence() {
    initialize();
    std::vector<PacketLog> run_logs;

    // 1. Discovery phase for all three targets
    run_logs.push_back(send_device_command(0x10, CommandId::CMD_DISCOVER));
    run_logs.push_back(send_device_command(0x20, CommandId::CMD_DISCOVER));
    run_logs.push_back(send_device_command(0x30, CommandId::CMD_DISCOVER));

    // 2. Capability Query phase
    run_logs.push_back(send_device_command(0x10, CommandId::CMD_QUERY_CAPS));
    run_logs.push_back(send_device_command(0x20, CommandId::CMD_QUERY_CAPS));
    run_logs.push_back(send_device_command(0x30, CommandId::CMD_QUERY_CAPS));

    // 3. Connect phase
    run_logs.push_back(send_device_command(0x10, CommandId::CMD_CONNECT));
    run_logs.push_back(send_device_command(0x20, CommandId::CMD_CONNECT));
    run_logs.push_back(send_device_command(0x30, CommandId::CMD_CONNECT));

    // 4. Command sequence 4: INJECT RECOVERABLE FAULT (CRC parity error on STM32)
    // Requirement: "Inject recoverable failure: simulated timeout or CRC parity error on command 4, followed by retry and recovery."
    run_logs.push_back(send_device_command(0x10, CommandId::CMD_REQ_TELEMETRY, true /* INJECT CRC FAULT */));

    // 5. Automatic Retry & Recovery on command 4
    run_logs.push_back(send_device_command(0x10, CommandId::CMD_FAULT_RECOVER));

    // 6. Normal telemetry requests on FPGA and JTAG
    run_logs.push_back(send_device_command(0x20, CommandId::CMD_REQ_TELEMETRY, false));
    run_logs.push_back(send_device_command(0x30, CommandId::CMD_REQ_TELEMETRY, false));

    // 7. Further steady state telemetry to verify healthy streaming
    run_logs.push_back(send_device_command(0x10, CommandId::CMD_REQ_TELEMETRY, false));
    run_logs.push_back(send_device_command(0x20, CommandId::CMD_REQ_TELEMETRY, false));

    return run_logs;
}

} // namespace Workbench
