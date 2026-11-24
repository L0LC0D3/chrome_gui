#pragma once

#include <cstddef>
#include <cstdint>

namespace chrome
{
    // The kinds of value a property grid row can show. A quat is four floats and is edited as euler angles in degrees.
    enum class field_type : std::uint8_t
    {
        f32,
        u32,
        boolean,
        vec3,
        quat,
        color3
    };

    // Largest value a field holds. A grid keeps the before and after bytes of an edit inline.
    inline constexpr std::uint32_t k_max_field_bytes = 16;

    [[nodiscard]] constexpr std::uint32_t field_size(field_type type) noexcept
    {
        switch (type)
        {
        case field_type::f32:
        case field_type::u32:
            return 4;
        case field_type::boolean:
            return 1;
        case field_type::vec3:
        case field_type::color3:
            return 12;
        case field_type::quat:
            return 16;
        }

        return 0;
    }

    // One field of a struct in memory: where it is and how to show it.
    struct field_desc
    {
        const char* name{nullptr};
        field_type type{field_type::f32};
        // Byte offset inside the struct.
        std::uint32_t offset{0};
    };

    [[nodiscard]] constexpr field_desc make_field(const char* name, field_type type, std::size_t offset) noexcept
    {
        return {name, type, static_cast<std::uint32_t>(offset)};
    }
}
