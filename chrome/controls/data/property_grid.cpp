#include <chrome/controls/data/property_grid.hpp>

#include <chrome/controls/core/draw.hpp>
#include <chrome/theme.hpp>

#include <chrome/controls/property/quat_field.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace chrome
{
    namespace
    {
        constexpr std::int32_t k_row_height = 24;
        constexpr std::int32_t k_padding = 8;
        constexpr std::int32_t k_radius = 3;
        constexpr std::int32_t k_cell_radius = 3;
        constexpr std::int32_t k_cell_gap = 4;
        constexpr std::int32_t k_indent = 16;
        constexpr std::int32_t k_swatch = 22;
        constexpr std::int32_t k_box = 16;
        constexpr std::int32_t k_axis_width = 12;
        constexpr std::int32_t k_bar_column = 10;
        constexpr std::int32_t k_thumb_width = 4;
        constexpr std::int32_t k_min_thumb = 16;
        constexpr std::int32_t k_wheel_rows = 3;
        constexpr std::int32_t k_split_reach = 3;
        // How far a press has to move before it scrubs instead of editing.
        constexpr std::int32_t k_scrub_distance = 3;
        constexpr float k_rad_to_deg = 57.29577951308232f;
        constexpr float k_deg_to_rad = 0.017453292519943295f;

        constexpr std::uint32_t k_key_tab = 0x09;
        constexpr std::uint32_t k_key_enter = 0x0D;
        constexpr std::uint32_t k_key_space = 0x20;
        constexpr std::uint32_t k_key_page_up = 0x21;
        constexpr std::uint32_t k_key_page_down = 0x22;
        constexpr std::uint32_t k_key_end = 0x23;
        constexpr std::uint32_t k_key_home = 0x24;
        constexpr std::uint32_t k_key_left = 0x25;
        constexpr std::uint32_t k_key_up = 0x26;
        constexpr std::uint32_t k_key_right = 0x27;
        constexpr std::uint32_t k_key_down = 0x28;
        constexpr std::uint32_t k_key_f2 = 0x71;

        constexpr std::uint32_t k_axis_x = 0xE06C6C;
        constexpr std::uint32_t k_axis_y = 0x7CC66B;
        constexpr std::uint32_t k_axis_z = 0x5B9BE6;

        [[nodiscard]] bool contains(const dock_rect& area, std::int32_t x, std::int32_t y) noexcept
        {
            return x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height;
        }

        // The name of a field as a label: no underscores, and a capital at the start.
        void label_of(const char* name, char* out, std::size_t size) noexcept
        {
            std::size_t i = 0;

            for (; name != nullptr && name[i] != '\0' && i + 1 < size; ++i)
                out[i] = name[i] == '_' ? ' ' : name[i];

            out[i] = '\0';

            if (i > 0 && out[0] >= 'a' && out[0] <= 'z')
                out[0] = static_cast<char>(out[0] - 'a' + 'A');
        }
    }

    property_grid::property_grid() noexcept
    {
        m_editor.set_buttons(false).set_on_change(&property_grid::editor_changed, this);
        const bool added = m_inner.add(m_editor);
        (void)added;
    }

    std::int32_t property_grid::add_group(const char* name, const field_desc* fields, std::uint32_t count, void* base, bool expanded) noexcept
    {
        if (m_group_count >= k_max_groups || name == nullptr || (count > 0 && fields == nullptr))
            return k_none;

        group& item = m_groups[m_group_count];
        item.name = name;
        item.fields = fields;
        item.count = count;
        item.base = base;
        item.expanded = expanded;
        rebuild();
        invalidate();
        return static_cast<std::int32_t>(m_group_count++);
    }


    property_grid& property_grid::clear() noexcept
    {
        end_edit();
        m_group_count = 0;
        m_current = k_none;
        m_first = 0;
        rebuild();
        invalidate();
        return *this;
    }

    property_grid& property_grid::set_base(std::uint32_t group_index, void* base) noexcept
    {
        if (group_index < m_group_count)
        {
            m_groups[group_index].base = base;
            invalidate();
        }

        return *this;
    }

    property_grid& property_grid::set_rows(std::uint32_t rows) noexcept
    {
        m_rows_shown = std::clamp<std::uint32_t>(rows, 1, 64);
        return *this;
    }

    property_grid& property_grid::set_split(float split) noexcept
    {
        m_split = std::clamp(split, 0.2f, 0.8f);
        place_editor();
        invalidate();
        return *this;
    }

    property_grid& property_grid::set_read_only(bool read_only) noexcept
    {
        m_read_only = read_only;

        if (read_only)
            end_edit();

        invalidate();
        return *this;
    }

    property_grid& property_grid::set_expanded(std::uint32_t group_index, bool expanded) noexcept
    {
        if (group_index < m_group_count && m_groups[group_index].expanded != expanded)
        {
            m_groups[group_index].expanded = expanded;
            rebuild();
            invalidate();
        }

        return *this;
    }

    std::int32_t property_grid::current_group() const noexcept
    {
        return m_current >= 0 && m_current < static_cast<std::int32_t>(m_row_count) ? m_rows[m_current].group : k_none;
    }

    std::int32_t property_grid::current_field() const noexcept
    {
        return m_current >= 0 && m_current < static_cast<std::int32_t>(m_row_count) && m_rows[m_current].field >= 0 ? m_rows[m_current].field : k_none;
    }

    void property_grid::rebuild() noexcept
    {
        end_edit();
        m_row_count = 0;

        for (std::uint32_t g = 0; g < m_group_count && m_row_count < k_max_rows; ++g)
        {
            m_rows[m_row_count++] = {static_cast<std::uint16_t>(g), -1};

            if (!m_groups[g].expanded)
                continue;

            for (std::uint32_t f = 0; f < m_groups[g].count && m_row_count < k_max_rows; ++f)
                m_rows[m_row_count++] = {static_cast<std::uint16_t>(g), static_cast<std::int16_t>(f)};
        }

        if (m_current >= static_cast<std::int32_t>(m_row_count))
            m_current = m_row_count > 0 ? static_cast<std::int32_t>(m_row_count) - 1 : k_none;

        m_hover_row = k_none;
        m_hover_cell = k_none;
        set_first(m_first);
    }

    const field_desc* property_grid::field_of(const row& item) const noexcept
    {
        return item.field >= 0 ? &m_groups[item.group].fields[item.field] : nullptr;
    }

    std::uint32_t property_grid::cell_count(field_type type) noexcept
    {
        switch (type)
        {
        case field_type::f32:
        case field_type::u32:
        case field_type::boolean:
            return 1;

        case field_type::vec3:
        case field_type::quat:
        case field_type::color3:
            return 3;
        }

        return 0;
    }

    bool property_grid::is_number(field_type type) noexcept
    {
        return type != field_type::boolean;
    }

    double property_grid::step_of(field_type type) noexcept
    {
        switch (type)
        {
        case field_type::u32:
            return 1.0;

        case field_type::quat:
            return 0.5;

        case field_type::color3:
            return 0.005;

        default:
            return 0.01;
        }
    }

    std::int32_t property_grid::decimals_of(field_type type) noexcept
    {
        switch (type)
        {
        case field_type::u32:
            return 0;

        case field_type::quat:
            return 2;

        default:
            return 3;
        }
    }

    bool property_grid::read_cell(const row& item, std::uint32_t cell, double& value) const noexcept
    {
        const field_desc* field = field_of(item);
        void* base = m_groups[item.group].base;

        if (field == nullptr || base == nullptr || cell >= cell_count(field->type))
            return false;

        const unsigned char* at = static_cast<const unsigned char*>(base) + field->offset;

        switch (field->type)
        {
        case field_type::f32:
        {
            float v = 0.0f;
            std::memcpy(&v, at, sizeof(v));
            value = v;
            return true;
        }

        case field_type::u32:
        {
            std::uint32_t v = 0;
            std::memcpy(&v, at, sizeof(v));
            value = v;
            return true;
        }

        case field_type::boolean:
            value = at[0] != 0 ? 1.0 : 0.0;
            return true;

        case field_type::vec3:
        case field_type::color3:
        {
            float v[3]{};
            std::memcpy(v, at, sizeof(v));
            value = v[cell];
            return true;
        }

        case field_type::quat:
        {
            quaternion q{};
            std::memcpy(&q, at, sizeof(q));
            const euler_angles e = euler_from_quaternion(q);
            const float degrees[3] = {e.pitch * k_rad_to_deg, e.yaw * k_rad_to_deg, e.roll * k_rad_to_deg};
            value = degrees[cell];
            return true;
        }
        }

        return false;
    }

    void property_grid::write_cell(const row& item, std::uint32_t cell, double value, bool final) noexcept
    {
        const field_desc* field = field_of(item);
        void* base = m_groups[item.group].base;

        if (field == nullptr || base == nullptr || m_read_only || cell >= cell_count(field->type) || !std::isfinite(value))
            return;

        const std::uint32_t size = field_size(field->type);
        unsigned char before[k_max_field_bytes]{};
        unsigned char bytes[k_max_field_bytes]{};
        std::memcpy(before, static_cast<const unsigned char*>(base) + field->offset, size);
        std::memcpy(bytes, before, size);

        switch (field->type)
        {
        case field_type::f32:
        {
            const float v = static_cast<float>(std::clamp(value, -1.0e9, 1.0e9));
            std::memcpy(bytes, &v, sizeof(v));
            break;
        }

        case field_type::u32:
        {
            const std::uint32_t v = static_cast<std::uint32_t>(std::clamp(std::round(value), 0.0, 4294967295.0));
            std::memcpy(bytes, &v, sizeof(v));
            break;
        }

        case field_type::boolean:
            bytes[0] = value != 0.0 ? 1 : 0;
            break;

        case field_type::vec3:
        case field_type::color3:
        {
            float v[3]{};
            std::memcpy(v, bytes, sizeof(v));
            v[cell] = static_cast<float>(field->type == field_type::color3 ? std::clamp(value, 0.0, 1000.0) : std::clamp(value, -1.0e9, 1.0e9));
            std::memcpy(bytes, v, sizeof(v));
            break;
        }

        case field_type::quat:
        {
            quaternion q{};
            std::memcpy(&q, bytes, sizeof(q));
            const euler_angles e = euler_from_quaternion(q);
            float degrees[3] = {e.pitch * k_rad_to_deg, e.yaw * k_rad_to_deg, e.roll * k_rad_to_deg};
            degrees[cell] = static_cast<float>(std::clamp(value, -36000.0, 36000.0));
            q = quaternion_from_euler(degrees[0] * k_deg_to_rad, degrees[1] * k_deg_to_rad, degrees[2] * k_deg_to_rad);
            std::memcpy(bytes, &q, sizeof(q));
            break;
        }
        }

        // An edit that changes nothing is dropped, but the end of a drag is always reported, so the owner can close it.
        if (!final && std::memcmp(before, bytes, size) == 0)
            return;

        send(item, bytes, final);
    }

    void property_grid::send(const row& item, const unsigned char* bytes, bool final) noexcept
    {
        if (m_edit != nullptr)
            m_edit(m_edit_user, *this, item.group, static_cast<std::uint32_t>(item.field), bytes, final);
        else
            apply(item.group, static_cast<std::uint32_t>(item.field), bytes);

        invalidate();
    }

    void property_grid::apply(std::uint32_t group_index, std::uint32_t field_index, const void* value) noexcept
    {
        if (group_index >= m_group_count || field_index >= m_groups[group_index].count || m_groups[group_index].base == nullptr || value == nullptr)
            return;

        const field_desc& field = m_groups[group_index].fields[field_index];
        std::memcpy(static_cast<unsigned char*>(m_groups[group_index].base) + field.offset, value, field_size(field.type));
        invalidate();
    }

    dock_rect property_grid::inner() const noexcept
    {
        const std::int32_t edge = draw::px(1);
        return {area().x + edge, area().y + edge, std::max(area().width - 2 * edge, 0), std::max(area().height - 2 * edge, 0)};
    }

    std::int32_t property_grid::visible_rows() const noexcept
    {
        return std::max(inner().height / std::max(draw::px(k_row_height), 1), 1);
    }

    std::int32_t property_grid::max_first() const noexcept
    {
        return std::max(static_cast<std::int32_t>(m_row_count) - visible_rows(), 0);
    }

    bool property_grid::bar_needed() const noexcept
    {
        return static_cast<std::int32_t>(m_row_count) > visible_rows();
    }

    std::int32_t property_grid::bar_left() const noexcept
    {
        const dock_rect box = inner();
        return box.x + box.width - draw::px(k_bar_column);
    }

    property_grid::thumb_span property_grid::thumb() const noexcept
    {
        const dock_rect box = inner();
        const std::int32_t rows = visible_rows();
        const std::int32_t count = std::max(static_cast<std::int32_t>(m_row_count), 1);
        thumb_span span;
        span.length = std::clamp(box.height * rows / count, std::min(draw::px(k_min_thumb), box.height), box.height);
        span.travel = box.height - span.length;
        const std::int32_t reach = max_first();
        span.start = box.y + (reach > 0 ? span.travel * m_first / reach : 0);
        return span;
    }

    std::int32_t property_grid::row_at(std::int32_t y) const noexcept
    {
        const dock_rect box = inner();

        if (y < box.y || y >= box.y + box.height)
            return k_none;

        const std::int32_t index = m_first + (y - box.y) / std::max(draw::px(k_row_height), 1);
        return index < static_cast<std::int32_t>(m_row_count) ? index : k_none;
    }

    std::int32_t property_grid::split_x() const noexcept
    {
        const dock_rect box = inner();
        const std::int32_t width = box.width - (bar_needed() ? draw::px(k_bar_column) : 0);
        return box.x + static_cast<std::int32_t>(static_cast<float>(width) * m_split);
    }

    dock_rect property_grid::row_rect(std::int32_t row_index) const noexcept
    {
        const dock_rect box = inner();
        const std::int32_t bar = bar_needed() ? draw::px(k_bar_column) : 0;
        return {box.x, box.y + (row_index - m_first) * draw::px(k_row_height), box.width - bar, draw::px(k_row_height)};
    }

    dock_rect property_grid::swatch_rect(std::int32_t row_index) const noexcept
    {
        const dock_rect line = row_rect(row_index);
        const std::int32_t size = draw::px(k_swatch);
        return {split_x() + draw::px(k_padding) - draw::px(2), line.y + (line.height - size) / 2, size, size};
    }

    dock_rect property_grid::cell_rect(std::int32_t row_index, std::uint32_t cell) const noexcept
    {
        const dock_rect line = row_rect(row_index);
        const field_desc* field = field_of(m_rows[row_index]);
        const std::int32_t left = split_x() + draw::px(k_padding) - draw::px(2);
        const std::int32_t right = line.x + line.width - draw::px(k_padding);
        const std::int32_t height = line.height - draw::px(4);

        if (field == nullptr)
            return {};

        if (field->type == field_type::boolean)
        {
            const std::int32_t box = draw::px(k_box);
            return {left, line.y + (line.height - box) / 2, box, box};
        }

        const std::uint32_t count = cell_count(field->type);
        const std::int32_t start = left + (field->type == field_type::color3 ? draw::px(k_swatch) + draw::px(k_cell_gap) : 0);
        const std::int32_t gap = draw::px(k_cell_gap);
        const std::int32_t width = std::max(right - start - static_cast<std::int32_t>(count - 1) * gap, 0) / static_cast<std::int32_t>(count);
        return {start + static_cast<std::int32_t>(cell) * (width + gap), line.y + 2, width, height};
    }

    std::int32_t property_grid::cell_at(std::int32_t row_index, std::int32_t x, std::int32_t y) const noexcept
    {
        const field_desc* field = field_of(m_rows[row_index]);

        if (field == nullptr)
            return k_none;

        const dock_rect line = row_rect(row_index);

        // A box is as easy to hit as the whole of the value side.
        if (field->type == field_type::boolean)
            return x >= split_x() && x < line.x + line.width && y >= line.y && y < line.y + line.height ? 0 : k_none;

        for (std::uint32_t c = 0; c < cell_count(field->type); ++c)
        {
            if (contains(cell_rect(row_index, c), x, y))
                return static_cast<std::int32_t>(c);
        }

        return k_none;
    }

    void property_grid::set_first(std::int32_t first) noexcept
    {
        const std::int32_t clamped = std::clamp(first, 0, max_first());

        if (clamped == m_first)
            return;

        m_first = clamped;
        place_editor();
        invalidate();
    }

    void property_grid::reveal_row(std::int32_t row_index) noexcept
    {
        if (row_index < 0)
            return;

        const std::int32_t rows = visible_rows();

        if (row_index < m_first)
            set_first(row_index);
        else if (row_index >= m_first + rows)
            set_first(row_index - rows + 1);
    }

    void property_grid::place_editor() noexcept
    {
        if (m_editing && m_edit_row >= 0 && m_edit_row < static_cast<std::int32_t>(m_row_count))
            m_editor.set_area(cell_rect(m_edit_row, m_edit_cell));
    }

    control_size property_grid::preferred_size(void* dc) const noexcept
    {
        return preferred_size_in(dc, 0);
    }

    control_size property_grid::preferred_size_in(void* dc, std::int32_t max_width) const noexcept
    {
        (void)dc;
        const std::int32_t width = m_width > 0 ? draw::px(m_width) : std::max(max_width, draw::px(160));
        return {width, static_cast<std::int32_t>(m_rows_shown) * draw::px(k_row_height) + 2 * draw::px(1)};
    }

    mouse_cursor property_grid::cursor_at(std::int32_t x, std::int32_t y) const noexcept
    {
        if (m_drag == drag_kind::split || m_drag == drag_kind::scrub)
            return mouse_cursor::resize_horizontal;

        if (m_drag != drag_kind::none)
            return mouse_cursor::arrow;

        const dock_rect box = inner();

        if (std::abs(x - split_x()) <= draw::px(k_split_reach) && y >= box.y && y < box.y + box.height)
            return mouse_cursor::resize_horizontal;

        const std::int32_t index = row_at(y);

        if (index != k_none && !m_read_only)
        {
            const field_desc* field = field_of(m_rows[index]);
            const std::int32_t cell = cell_at(index, x, y);

            // A number cell scrubs, which is a sideways drag, unless it is the one being edited.
            if (field != nullptr && cell != k_none && is_number(field->type) && !(m_editing && m_edit_row == index && m_edit_cell == static_cast<std::uint32_t>(cell)))
                return mouse_cursor::resize_horizontal;
        }

        return mouse_cursor::arrow;
    }

    void property_grid::start_edit(std::int32_t row_index, std::uint32_t cell) noexcept
    {
        if (m_read_only || row_index < 0 || row_index >= static_cast<std::int32_t>(m_row_count))
            return;

        const row item = m_rows[row_index];
        const field_desc* field = field_of(item);
        double value = 0.0;

        if (field == nullptr || !is_number(field->type) || !read_cell(item, cell, value))
            return;

        end_edit();

        switch (field->type)
        {
        case field_type::u32:
            m_editor.set_range(0.0, 4294967295.0);
            break;

        case field_type::quat:
            m_editor.set_range(-36000.0, 36000.0);
            break;

        case field_type::color3:
            m_editor.set_range(0.0, 1000.0);
            break;

        default:
            m_editor.set_range(-1.0e9, 1.0e9);
            break;
        }

        m_editor.set_step(step_of(field->type)).set_decimals(decimals_of(field->type)).set_value(value);
        m_edit_row = row_index;
        m_edit_cell = cell;
        m_current = row_index;
        m_cell = cell;
        m_editing = true;
        place_editor();
        m_inner.set_focus(&m_editor);
        invalidate();
    }

    void property_grid::end_edit() noexcept
    {
        if (!m_editing)
            return;

        // Losing the focus commits what was typed.
        m_inner.blur();
        settle_edit();
    }

    void property_grid::settle_edit() noexcept
    {
        if (m_editing && m_inner.focused() == nullptr)
        {
            m_editing = false;
            m_edit_row = k_none;
            invalidate();
        }
    }

    void property_grid::editor_changed(void* user, number_field& source) noexcept
    {
        auto& self = *static_cast<property_grid*>(user);

        if (self.m_editing && self.m_edit_row >= 0 && self.m_edit_row < static_cast<std::int32_t>(self.m_row_count))
            self.write_cell(self.m_rows[self.m_edit_row], self.m_edit_cell, source.value(), true);
    }

    void property_grid::toggle_group(std::uint32_t group_index) noexcept
    {
        if (group_index >= m_group_count)
            return;

        m_groups[group_index].expanded = !m_groups[group_index].expanded;
        rebuild();

        // The grid stays on the bar that was folded.
        for (std::uint32_t i = 0; i < m_row_count; ++i)
        {
            if (m_rows[i].group == group_index && m_rows[i].field < 0)
                m_current = static_cast<std::int32_t>(i);
        }

        invalidate();
    }

    void property_grid::toggle_box(const row& item) noexcept
    {
        double value = 0.0;

        if (!m_read_only && read_cell(item, 0, value))
            write_cell(item, 0, value != 0.0 ? 0.0 : 1.0, true);
    }

    bool property_grid::next_cell(std::int32_t direction, std::int32_t& row_index, std::uint32_t& cell) const noexcept
    {
        std::int32_t r = row_index;
        std::int32_t c = static_cast<std::int32_t>(cell) + direction;

        while (r >= 0 && r < static_cast<std::int32_t>(m_row_count))
        {
            const field_desc* field = field_of(m_rows[r]);

            if (field != nullptr && is_number(field->type))
            {
                const std::int32_t count = static_cast<std::int32_t>(cell_count(field->type));

                if (c < 0 && direction < 0)
                    c = count - 1;
                else if (c >= count && direction > 0)
                    c = count;

                if (c >= 0 && c < count)
                {
                    row_index = r;
                    cell = static_cast<std::uint32_t>(c);
                    return true;
                }
            }

            // On to the next row, from its first cell or its last.
            r += direction;
            c = direction > 0 ? 0 : 1000;
        }

        return false;
    }

    void property_grid::on_press(std::int32_t x, std::int32_t y) noexcept
    {
        m_drag = drag_kind::none;
        m_x = x;
        m_y = y;

        if (m_editing)
        {
            if (contains(m_editor.area(), x, y))
            {
                panel_mouse mouse;
                mouse.event = mouse_event::down;
                mouse.x = x;
                mouse.y = y;
                mouse.ctrl = ctrl_down();
                mouse.shift = shift_down();
                (void)m_inner.on_mouse(mouse);
                m_drag = drag_kind::editor;
                invalidate();
                return;
            }

            // A click anywhere else ends the edit, and then does what it would have done.
            end_edit();
        }

        const dock_rect box = inner();

        if (bar_needed() && x >= bar_left())
        {
            const thumb_span span = thumb();

            if (y >= span.start && y < span.start + span.length)
            {
                m_drag = drag_kind::bar;
                m_grab = y - span.start;
            }
            else
            {
                set_first(m_first + (y < span.start ? -1 : 1) * std::max(visible_rows() - 1, 1));
            }

            invalidate();
            return;
        }

        if (std::abs(x - split_x()) <= draw::px(k_split_reach) && y >= box.y && y < box.y + box.height)
        {
            m_drag = drag_kind::split;
            return;
        }

        const std::int32_t index = row_at(y);

        if (index == k_none)
            return;

        m_current = index;
        const row item = m_rows[index];
        invalidate();

        if (item.field < 0)
        {
            toggle_group(item.group);
            return;
        }

        const std::int32_t cell = cell_at(index, x, y);

        if (cell == k_none)
            return;

        m_cell = static_cast<std::uint32_t>(cell);
        const field_desc* field = field_of(item);

        if (m_read_only || field == nullptr)
            return;

        if (field->type == field_type::boolean)
        {
            toggle_box(item);
            return;
        }

        // The press waits to see whether it is a click, which edits, or a drag, which scrubs.
        m_drag = drag_kind::cell;
        m_press_x = x;
        m_press_row = index;
        m_press_cell = m_cell;
        m_press_value = 0.0;
        (void)read_cell(item, m_cell, m_press_value);
        m_scrub_from = m_press_value;
    }

    void property_grid::on_double_click(std::int32_t x, std::int32_t y) noexcept
    {
        const std::int32_t index = row_at(y);

        // A second click on a bar is not another fold.
        if (!m_editing && index != k_none && m_rows[index].field < 0)
            return;

        on_press(x, y);
    }

    bool property_grid::on_drag(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;

        if (m_drag == drag_kind::editor)
        {
            panel_mouse mouse;
            mouse.event = mouse_event::move;
            mouse.x = x;
            mouse.y = y;
            mouse.ctrl = ctrl_down();
            mouse.shift = shift_down();
            (void)m_inner.on_mouse(mouse);
            return true;
        }

        if (m_drag == drag_kind::bar)
        {
            const thumb_span span = thumb();
            const std::int32_t at = std::clamp(y - m_grab - inner().y, 0, std::max(span.travel, 0));
            const std::int32_t before = m_first;
            set_first(span.travel > 0 ? (at * max_first() + span.travel / 2) / span.travel : 0);
            return m_first != before;
        }

        if (m_drag == drag_kind::split)
        {
            const dock_rect box = inner();
            const std::int32_t width = std::max(box.width - (bar_needed() ? draw::px(k_bar_column) : 0), 1);
            set_split(static_cast<float>(x - box.x) / static_cast<float>(width));
            return true;
        }

        if (m_drag == drag_kind::cell && std::abs(x - m_press_x) >= draw::px(k_scrub_distance))
            m_drag = drag_kind::scrub;

        if (m_drag == drag_kind::scrub && m_press_row >= 0 && m_press_row < static_cast<std::int32_t>(m_row_count))
        {
            const row item = m_rows[m_press_row];
            const field_desc* field = field_of(item);

            if (field == nullptr)
                return false;

            // A pixel is a step, ten times that with Shift and a tenth with Ctrl.
            const double scale = step_of(field->type) * (shift_down() ? 10.0 : ctrl_down() ? 0.1 : 1.0);
            m_scrub_from = m_press_value + static_cast<double>(x - m_press_x) * scale;
            write_cell(item, m_press_cell, m_scrub_from, false);
            return true;
        }

        return false;
    }

    bool property_grid::on_move(std::int32_t x, std::int32_t y) noexcept
    {
        m_x = x;
        m_y = y;
        const std::int32_t index = bar_needed() && x >= bar_left() ? k_none : row_at(y);
        const std::int32_t cell = index != k_none ? cell_at(index, x, y) : k_none;
        bool changed = false;

        if (m_editing)
        {
            panel_mouse mouse;
            mouse.event = mouse_event::move;
            mouse.x = x;
            mouse.y = y;
            changed = m_inner.on_mouse(mouse);
        }

        if (index == m_hover_row && cell == m_hover_cell)
            return changed;

        m_hover_row = index;
        m_hover_cell = cell;
        return true;
    }

    void property_grid::on_release() noexcept
    {
        const drag_kind kind = m_drag;
        m_drag = drag_kind::none;

        if (kind == drag_kind::editor)
        {
            panel_mouse mouse;
            mouse.event = mouse_event::up;
            mouse.x = m_x;
            mouse.y = m_y;
            mouse.ctrl = ctrl_down();
            mouse.shift = shift_down();
            (void)m_inner.on_mouse(mouse);
            settle_edit();
        }
        else if (kind == drag_kind::scrub && m_press_row >= 0 && m_press_row < static_cast<std::int32_t>(m_row_count))
        {
            // The drag is over, so the owner can fold what it saw into one edit.
            write_cell(m_rows[m_press_row], m_press_cell, m_scrub_from, true);
        }
        else if (kind == drag_kind::cell)
        {
            start_edit(m_press_row, m_press_cell);
        }

        invalidate();
    }

    bool property_grid::on_wheel(std::int32_t x, std::int32_t y, std::int32_t notches) noexcept
    {
        (void)x;
        (void)y;
        const std::int32_t before = m_first;
        set_first(m_first - notches * k_wheel_rows);
        return m_first != before;
    }

    bool property_grid::on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept
    {
        if (m_editing)
        {
            if (key == k_key_tab && !ctrl && !alt)
            {
                // Commits this cell and edits the next one. At the ends the focus moves on to the next control.
                std::int32_t next_row = m_edit_row;
                std::uint32_t next = m_edit_cell;
                const bool found = next_cell(shift ? -1 : 1, next_row, next);
                end_edit();

                if (!found)
                    return false;

                m_cell = next;
                reveal_row(next_row);
                start_edit(next_row, next);
                return true;
            }

            const bool used = m_inner.on_key(key, ctrl, shift, alt);
            settle_edit();
            invalidate();
            return used;
        }

        if (alt || m_row_count == 0)
            return false;

        const std::int32_t last = static_cast<std::int32_t>(m_row_count) - 1;
        const std::int32_t page = std::max(visible_rows() - 1, 1);
        const std::int32_t from = m_current >= 0 ? m_current : -1;
        std::int32_t target = m_current;

        switch (key)
        {
        case k_key_up:
            target = from < 0 ? 0 : std::max(from - 1, 0);
            break;

        case k_key_down:
            target = from < 0 ? 0 : std::min(from + 1, last);
            break;

        case k_key_page_up:
            target = std::max(from - page, 0);
            break;

        case k_key_page_down:
            target = std::min(from + page, last);
            break;

        case k_key_home:
            target = 0;
            break;

        case k_key_end:
            target = last;
            break;

        case k_key_left:
        case k_key_right:
        {
            if (m_current < 0)
                return false;

            const row item = m_rows[m_current];

            // On a bar these fold and unfold, and on a field they move between its cells.
            if (item.field < 0)
            {
                if (m_groups[item.group].expanded == (key == k_key_left))
                    toggle_group(item.group);

                return true;
            }

            const std::uint32_t cells = cell_count(field_of(item)->type);

            if (key == k_key_left)
                m_cell = m_cell > 0 ? m_cell - 1 : 0;
            else
                m_cell = std::min(m_cell + 1, cells - 1);

            invalidate();
            return true;
        }

        case k_key_enter:
        case k_key_f2:
        case k_key_space:
        {
            if (m_current < 0)
                return false;

            const row item = m_rows[m_current];

            if (item.field < 0)
            {
                toggle_group(item.group);
                return true;
            }

            if (field_of(item)->type == field_type::boolean)
                toggle_box(item);
            else if (key != k_key_space)
                start_edit(m_current, std::min(m_cell, cell_count(field_of(item)->type) - 1));

            return true;
        }

        default:
            return false;
        }

        if (target != m_current)
        {
            m_current = target;
            const row item = m_rows[m_current];
            m_cell = item.field >= 0 ? std::min(m_cell, cell_count(field_of(item)->type) - 1) : 0;
            reveal_row(m_current);
        }

        invalidate();
        return true;
    }

    void property_grid::on_char(std::uint32_t code) noexcept
    {
        if (!m_editing)
            return;

        m_inner.on_char(code);
        settle_edit();
        invalidate();
    }

    void property_grid::on_focus(bool gained) noexcept
    {
        if (!gained)
            end_edit();

        invalidate();
    }

    bool property_grid::animate(std::uint64_t now_ms) noexcept
    {
        if (!m_editing)
            return false;

        (void)m_inner.animate(now_ms);

        if (m_inner.take_dirty())
            invalidate();

        return true;
    }

    void property_grid::paint(void* dc) const noexcept
    {
        const theme_colours& colours = theme();
        const std::uint32_t black = 0x000000;
        const std::uint32_t white = 0xFFFFFF;
        const bool lit = focused();
        std::uint32_t face = blend_colour(colours.panel, black, 0.14f);
        std::uint32_t edge = lit ? colours.accent : blend_colour(colours.title, black, 0.5f);
        std::uint32_t ink = colours.text;
        std::uint32_t dim = colours.text_dim;

        if (!enabled())
        {
            face = blend_colour(colours.panel, colours.title, 0.5f);
            edge = blend_colour(colours.panel, black, 0.2f);
            ink = blend_colour(colours.text_dim, colours.panel, 0.45f);
            dim = ink;
        }

        draw::rounded(dc, area(), k_radius, face, edge);

        const dock_rect box = inner();
        draw::clip(dc, box);

        if (m_row_count == 0 && m_empty != nullptr)
            draw::text(dc, box, m_empty, colours.text_dim, draw::text_center);

        const std::int32_t row_height = draw::px(k_row_height);
        const std::int32_t padding = draw::px(k_padding);
        const std::int32_t bar = bar_needed() ? draw::px(k_bar_column) : 0;
        const std::int32_t split = split_x();
        const std::int32_t rows = visible_rows();
        const std::uint32_t header_face = blend_colour(colours.panel, colours.title, 0.9f);
        const std::uint32_t cell_face = blend_colour(face, colours.hover, 0.35f);
        const std::uint32_t rule = blend_colour(face, white, 0.06f);
        const bool read_only = m_read_only || !enabled();

        for (std::int32_t i = 0; i < rows + 1; ++i)
        {
            const std::int32_t index = m_first + i;

            if (index >= static_cast<std::int32_t>(m_row_count))
                break;

            const row item = m_rows[index];
            const dock_rect line = row_rect(index);
            const bool current = index == m_current;
            const bool hovered_row = hot() && enabled() && index == m_hover_row;

            if (item.field < 0)
            {
                // A group's bar: a chevron and the component's name.
                draw::fill(dc, line, header_face);

                if (current && lit)
                    draw::rounded(dc, line, 2, header_face, colours.accent);
                else if (hovered_row)
                    draw::fill(dc, line, blend_colour(header_face, colours.hover, 0.7f));

                const bool open = m_groups[item.group].expanded;
                const float cx = static_cast<float>(line.x + padding) + static_cast<float>(draw::px(4));
                const float cy = static_cast<float>(line.y) + static_cast<float>(line.height) * 0.5f;
                const float half = static_cast<float>(draw::px(4)) * 0.8f;

                if (open)
                {
                    const draw::point down[3] = {{cx - half, cy - half * 0.5f}, {cx, cy + half * 0.5f}, {cx + half, cy - half * 0.5f}};
                    draw::polyline(dc, down, 3, dim, 1.5f);
                }
                else
                {
                    const draw::point right[3] = {{cx - half * 0.5f, cy - half}, {cx + half * 0.5f, cy}, {cx - half * 0.5f, cy + half}};
                    draw::polyline(dc, right, 3, dim, 1.5f);
                }

                const std::int32_t text_left = line.x + padding + draw::px(k_indent);
                draw::text(dc, {text_left, line.y, std::max(line.x + line.width - text_left - padding, 0), line.height}, m_groups[item.group].name, ink,
                    draw::text_bold | draw::text_ellipsis);
                continue;
            }

            const field_desc& field = m_groups[item.group].fields[item.field];

            if (current)
            {
                const std::uint32_t tint = lit ? blend_colour(colours.accent, face, 0.65f) : blend_colour(face, colours.hover, 0.5f);
                draw::fill(dc, {line.x, line.y, split - line.x, line.height}, tint);
            }
            else if (hovered_row)
            {
                draw::fill(dc, {line.x, line.y, split - line.x, line.height}, blend_colour(face, colours.hover, 0.4f));
            }

            char label[48];
            label_of(field.name, label, sizeof(label));
            const std::int32_t name_left = line.x + padding + draw::px(k_indent);
            draw::text(dc, {name_left, line.y, std::max(split - name_left - padding, 0), line.height}, label, ink, draw::text_ellipsis);

            const unsigned char* at = m_groups[item.group].base != nullptr ? static_cast<const unsigned char*>(m_groups[item.group].base) + field.offset : nullptr;

            if (field.type == field_type::boolean)
            {
                const dock_rect check = cell_rect(index, 0);
                const bool on = at != nullptr && at[0] != 0;
                const bool over = hot() && !read_only && index == m_hover_row;
                std::uint32_t box_face = on ? colours.accent : cell_face;
                std::uint32_t box_edge = over ? colours.accent : blend_colour(colours.title, black, 0.5f);

                if (read_only)
                {
                    box_face = on ? blend_colour(colours.accent, face, 0.55f) : face;
                    box_edge = blend_colour(face, black, 0.2f);
                }

                draw::rounded(dc, check, 3, box_face, box_edge);

                if (on)
                    draw::icon(dc, icon_kind::check, check, white);

                continue;
            }

            if (field.type == field_type::color3 && at != nullptr)
            {
                float rgb[3]{};
                std::memcpy(rgb, at, sizeof(rgb));
                const auto channel = [](float v) { return static_cast<std::uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
                const std::uint32_t colour = (channel(rgb[0]) << 16) | (channel(rgb[1]) << 8) | channel(rgb[2]);
                draw::rounded(dc, swatch_rect(index), 3, colour, blend_colour(colours.title, black, 0.5f));
            }

            const std::uint32_t cells = cell_count(field.type);

            for (std::uint32_t c = 0; c < cells; ++c)
            {
                const dock_rect cell = cell_rect(index, c);
                const bool editing_this = m_editing && m_edit_row == index && m_edit_cell == c;

                if (editing_this)
                    continue;

                const bool over = hot() && !read_only && index == m_hover_row && static_cast<std::int32_t>(c) == m_hover_cell;
                const bool cursor = lit && current && c == m_cell;
                std::uint32_t outline = cursor ? colours.accent : over ? blend_colour(colours.accent, face, 0.4f) : blend_colour(face, black, 0.25f);

                draw::rounded(dc, cell, k_cell_radius, over ? blend_colour(cell_face, colours.hover, 0.5f) : cell_face, outline);

                double value = 0.0;
                std::int32_t text_left = cell.x + draw::px(6);

                // Three cells carry the letter of their axis.
                if (cells == 3)
                {
                    static const char* const k_xyz[] = {"X", "Y", "Z"};
                    static const char* const k_rgb[] = {"R", "G", "B"};
                    static const std::uint32_t k_axis[] = {k_axis_x, k_axis_y, k_axis_z};
                    const char* letter = field.type == field_type::color3 ? k_rgb[c] : k_xyz[c];
                    draw::text(dc, {cell.x + draw::px(5), cell.y, draw::px(k_axis_width), cell.height}, letter, read_only ? dim : k_axis[c]);
                    text_left = cell.x + draw::px(5) + draw::px(k_axis_width);
                }

                if (!read_cell(row{item.group, item.field}, c, value))
                    continue;

                char text[40];

                if (field.type == field_type::u32)
                    std::snprintf(text, sizeof(text), "%.0f", value);
                else
                    std::snprintf(text, sizeof(text), field.type == field_type::quat ? "%.2f" : "%.3f", value);

                draw::text(dc, {text_left, cell.y, std::max(cell.x + cell.width - text_left - draw::px(4), 0), cell.height}, text, read_only ? dim : ink,
                    draw::text_ellipsis);
            }
        }

        // The line between the names and the values.
        if (m_show_rule)
            draw::fill(dc, {split, box.y, 1, box.height}, rule);

        if (m_editing)
            m_inner.paint(dc);

        if (bar > 0)
        {
            const thumb_span span = thumb();
            const std::int32_t width = draw::px(k_thumb_width);
            const bool grabbed = m_drag == drag_kind::bar;
            const std::uint32_t thumb_colour = grabbed ? colours.text_dim : blend_colour(colours.text_dim, face, hot() ? 0.2f : 0.45f);
            draw::rounded(dc, {box.x + box.width - (bar + width) / 2, span.start, width, span.length}, 2, thumb_colour, thumb_colour);
        }

        (void)row_height;
        draw::unclip(dc);
    }
}
