#include "gimbal_impl.h"
#include "global_include.h"
#include "gimbal_protocol_v1.h"
#include <functional>
#include <cmath>

namespace mavsdk {

GimbalImpl::GimbalImpl(System& system) : PluginImplBase(system)
{
    _parent->register_plugin(this);
}

GimbalImpl::~GimbalImpl()
{
    _parent->unregister_plugin(this);
}

void GimbalImpl::init()
{
    _parent->register_mavlink_message_handler(
        MAVLINK_MSG_ID_GIMBAL_MANAGER_INFORMATION,
        [this](const mavlink_message_t& message) { process_gimbal_manager_information(message); },
        this);
}

void GimbalImpl::deinit() {}

void GimbalImpl::enable()
{
    _parent->register_timeout_handler(
        [this]() { receive_protocol_timeout(); }, 1.0, &_protocol_cookie);

    MAVLinkCommands::CommandLong command{};
    command.command = MAV_CMD_REQUEST_MESSAGE;
    command.params.param1 = static_cast<float>(MAVLINK_MSG_ID_GIMBAL_MANAGER_INFORMATION);
    command.target_component_id = 0; // any component
    _parent->send_command_async(command, nullptr);
}

void GimbalImpl::disable()
{
    _gimbal_protocol.reset(nullptr);
}

void GimbalImpl::receive_protocol_timeout()
{
    // We did not receive a GIMBAL_MANAGER_INFORMATION in time, so we have to
    // assume Version2 is not available.
    LogDebug() << "Falling back to Gimbal Version 1";
    _gimbal_protocol.reset(new GimbalProtocolV1(*_parent));
}

void GimbalImpl::process_gimbal_manager_information(const mavlink_message_t& message)
{
    mavlink_gimbal_manager_information_t gimbal_manager_information;
    mavlink_msg_gimbal_manager_information_decode(&message, &gimbal_manager_information);

    LogDebug() << "Using Gimbal Version 2 as gimbal manager for gimbal device "
               << static_cast<int>(gimbal_manager_information.gimbal_device_id)
               << " was discovered";

    //_gimbal_protocol.reset(new GimbalProtocolV2(*_parent));
}

Gimbal::Result GimbalImpl::set_pitch_and_yaw(float pitch_deg, float yaw_deg)
{
    const float roll_deg = 0.0f;
    MAVLinkCommands::CommandLong command{};

    command.command = MAV_CMD_DO_MOUNT_CONTROL;
    command.params.param1 = pitch_deg;
    command.params.param2 = roll_deg;
    command.params.param3 = yaw_deg;
    command.params.param7 = float(MAV_MOUNT_MODE_MAVLINK_TARGETING);
    command.target_component_id = _parent->get_autopilot_id();

    return gimbal_result_from_command_result(_parent->send_command(command));
}

void GimbalImpl::set_pitch_and_yaw_async(
    float pitch_deg, float yaw_deg, Gimbal::ResultCallback callback)
{
    const float roll_deg = 0.0f;
    MAVLinkCommands::CommandLong command{};

    command.command = MAV_CMD_DO_MOUNT_CONTROL;
    command.params.param1 = pitch_deg;
    command.params.param2 = roll_deg;
    command.params.param3 = yaw_deg;
    command.params.param7 = float(MAV_MOUNT_MODE_MAVLINK_TARGETING);
    command.target_component_id = _parent->get_autopilot_id();

    _parent->send_command_async(
        command, [callback](MAVLinkCommands::Result command_result, float progress) {
            UNUSED(progress);
            receive_command_result(command_result, callback);
        });
}

Gimbal::Result GimbalImpl::set_mode(const Gimbal::GimbalMode gimbal_mode)
{
    if (_gimbal_protocol) {
        return _gimbal_protocol->set_mode(gimbal_mode);
    }

    // FIXME: should be
    // return Gimbal::Result::ProtocolUnknown
    return Gimbal::Result::Error;
}

void GimbalImpl::set_mode_async(
    const Gimbal::GimbalMode gimbal_mode, Gimbal::ResultCallback callback)
{
    if (_gimbal_protocol) {
        _gimbal_protocol->set_mode_async(gimbal_mode, callback);
        return;
    }

    if (callback) {
        auto temp_callback = callback;
        _parent->call_user_callback([temp_callback]() {
            // FIXME: should be
            // temp_callback(Gimbal::Result::ProtocolUnknown)
            temp_callback(Gimbal::Result::Error);
        });
        return;
    }
}

Gimbal::Result
GimbalImpl::set_roi_location(double latitude_deg, double longitude_deg, float altitude_m)
{
    if (_gimbal_protocol) {
        return _gimbal_protocol->set_roi_location(latitude_deg, longitude_deg, altitude_m);
    }

    // FIXME: should be
    // return Gimbal::Result::ProtocolUnknown
    return Gimbal::Result::Error;
}

void GimbalImpl::set_roi_location_async(
    double latitude_deg, double longitude_deg, float altitude_m, Gimbal::ResultCallback callback)
{
    if (_gimbal_protocol) {
        _gimbal_protocol->set_roi_location_async(latitude_deg, longitude_deg, altitude_m, callback);
        return;
    }

    if (callback) {
        auto temp_callback = callback;
        _parent->call_user_callback([temp_callback]() {
            // FIXME: should be
            // temp_callback(Gimbal::Result::ProtocolUnknown)
            temp_callback(Gimbal::Result::Error);
        });
        return;
    }
}

void GimbalImpl::receive_command_result(
    MAVLinkCommands::Result command_result, const Gimbal::ResultCallback& callback)
{
    Gimbal::Result gimbal_result = gimbal_result_from_command_result(command_result);

    if (callback) {
        callback(gimbal_result);
    }
}

Gimbal::Result GimbalImpl::gimbal_result_from_command_result(MAVLinkCommands::Result command_result)
{
    switch (command_result) {
        case MAVLinkCommands::Result::Success:
            return Gimbal::Result::Success;
        case MAVLinkCommands::Result::Timeout:
            return Gimbal::Result::Timeout;
        case MAVLinkCommands::Result::NoSystem:
        case MAVLinkCommands::Result::ConnectionError:
        case MAVLinkCommands::Result::Busy:
        case MAVLinkCommands::Result::CommandDenied:
        default:
            return Gimbal::Result::Error;
    }
}

} // namespace mavsdk
