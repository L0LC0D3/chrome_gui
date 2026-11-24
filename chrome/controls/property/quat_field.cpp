#include <chrome/controls/property/quat_field.hpp>

#include <cmath>

namespace chrome
{
    namespace
    {
        constexpr float k_rad_to_deg = 57.29577951308232f;
        constexpr float k_deg_to_rad = 0.017453292519943295f;
    }

    quat_field::quat_field() noexcept
        : vec_field(3)
    {
        vec_field::set_on_change(&quat_field::edited, this);
        vec_field::set_range(-36000.0, 36000.0).set_step(1.0).set_decimals(2);
    }

    void quat_field::rebuild() noexcept
    {
        m_rotation = quaternion_from_euler(static_cast<float>(value(0)) * k_deg_to_rad, static_cast<float>(value(1)) * k_deg_to_rad, static_cast<float>(value(2)) * k_deg_to_rad);
    }

    quat_field& quat_field::set_quat(quaternion rotation) noexcept
    {
        m_rotation = rotation;
        const euler_angles angles = euler_from_quaternion(rotation);
        (void)vec_field::set_value(0, static_cast<double>(angles.pitch * k_rad_to_deg));
        (void)vec_field::set_value(1, static_cast<double>(angles.yaw * k_rad_to_deg));
        (void)vec_field::set_value(2, static_cast<double>(angles.roll * k_rad_to_deg));
        return *this;
    }

    quat_field& quat_field::set_euler(double pitch, double yaw, double roll) noexcept
    {
        (void)vec_field::set_value(0, pitch);
        (void)vec_field::set_value(1, yaw);
        (void)vec_field::set_value(2, roll);
        rebuild();
        return *this;
    }

    void quat_field::edited(void* user, vec_field& source, std::uint32_t index) noexcept
    {
        (void)source;
        (void)index;
        auto& self = *static_cast<quat_field*>(user);
        self.rebuild();

        if (self.m_handler != nullptr)
            self.m_handler(self.m_user, self);
    }

    quaternion quaternion_from_euler(float pitch, float yaw, float roll) noexcept
    {
        const float sp = std::sin(pitch * 0.5f);
        const float cp = std::cos(pitch * 0.5f);
        const float sy = std::sin(yaw * 0.5f);
        const float cy = std::cos(yaw * 0.5f);
        const float sr = std::sin(roll * 0.5f);
        const float cr = std::cos(roll * 0.5f);
        return {
            cy * sp * cr + sy * cp * sr,
            sy * cp * cr - cy * sp * sr,
            cy * cp * sr - sy * sp * cr,
            cy * cp * cr + sy * sp * sr};
    }

    euler_angles euler_from_quaternion(quaternion value) noexcept
    {
        const float len_sq = value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w;

        if (len_sq == 0.0f)
            return {};

        const float inv_len = 1.0f / std::sqrt(len_sq);
        value = {value.x * inv_len, value.y * inv_len, value.z * inv_len, value.w * inv_len};
        const float sin_pitch = std::fmax(-1.0f, std::fmin(1.0f, 2.0f * (value.w * value.x - value.y * value.z)));
        const float pitch = std::asin(sin_pitch);

        if (sin_pitch > 0.99999f || sin_pitch < -0.99999f)
        {
            const float m00 = 1.0f - 2.0f * (value.y * value.y + value.z * value.z);
            const float m01 = 2.0f * (value.x * value.y - value.w * value.z);
            return {pitch, sin_pitch > 0.0f ? std::atan2(m01, m00) : std::atan2(-m01, m00), 0.0f};
        }

        const float yaw = std::atan2(2.0f * (value.x * value.z + value.w * value.y), 1.0f - 2.0f * (value.x * value.x + value.y * value.y));
        const float roll = std::atan2(2.0f * (value.x * value.y + value.w * value.z), 1.0f - 2.0f * (value.x * value.x + value.z * value.z));
        return {pitch, yaw, roll};
    }
}
