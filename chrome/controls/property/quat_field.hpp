#pragma once

#include <chrome/controls/property/vec_field.hpp>

#include <cstdint>

namespace chrome
{
    // A rotation as plain numbers, so the control needs no math library. w is the real part.
    struct quaternion
    {
        float x{0.0f};
        float y{0.0f};
        float z{0.0f};
        float w{1.0f};
    };

    struct euler_angles
    {
        float pitch{0.0f};
        float yaw{0.0f};
        float roll{0.0f};
    };

    // Intrinsic YXZ, radians: pitch about X, yaw about Y and roll about Z.
    [[nodiscard]] quaternion quaternion_from_euler(float pitch, float yaw, float roll) noexcept;
    [[nodiscard]] euler_angles euler_from_quaternion(quaternion value) noexcept;

    // A rotation, shown and edited as three euler angles in degrees, in the order quaternion_from_euler takes them. It is a
    // vec3 field, so it has the letters, the label, the typing and the drag on a letter of one, and a quaternion behind it.
    //
    // The angles are what the user typed and are kept as they are, so editing one does not move the other two, which a
    // round trip through the quaternion would do whenever an angle goes past 90 degrees. The quaternion is made from the
    // three angles after each edit. Setting it from code shows the angles of that rotation.
    //
    //   quat_field rotation;
    //   rotation.set_label("Rotation");
    //   rotation.set_on_change(&turned, this);
    //   rotation.set_quat(quaternion_from_euler(0.0f, 0.785f, 0.0f));
    //   set.add(rotation);
    class quat_field final : public vec_field
    {
    public:
        // Called after the user changed an angle, with the new rotation in rotation().
        using change_fn = void (*)(void* user, quat_field& source) noexcept;

        quat_field() noexcept;

        // Shows the angles of a rotation. Setting it does not call the handler.
        quat_field& set_quat(quaternion rotation) noexcept;
        // Shows the angles given, in degrees, and makes the rotation from them.
        quat_field& set_euler(double pitch, double yaw, double roll) noexcept;
        [[nodiscard]] quaternion rotation() const noexcept { return m_rotation; }
        [[nodiscard]] double pitch() const noexcept { return value(0); }
        [[nodiscard]] double yaw() const noexcept { return value(1); }
        [[nodiscard]] double roll() const noexcept { return value(2); }

        quat_field& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        // The angles are the value, so they are set through set_euler, and the handler through the one above.
        bool set_value(std::uint32_t, double) = delete;
        vec_field& set_on_change(vec_field::change_fn, void*) = delete;

    private:
        static void edited(void* user, vec_field& source, std::uint32_t index) noexcept;
        void rebuild() noexcept;

        quaternion m_rotation{};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};
    };
}
