#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    enum class check_state : std::uint8_t
    {
        unchecked,
        checked,
        // Neither: some of what the checkbox stands for is on and some is off. Only a tri-state checkbox goes there by
        // clicking, but any can be set to it.
        mixed
    };

    enum class checkbox_style : std::uint8_t
    {
        // A square with a tick in it.
        box,
        // A pill with a knob that sits at one end or the other.
        switch_pill
    };

    // A box that is ticked or not, or a switch, with an optional label on either side.
    //
    //   checkbox snap;
    //   snap.set_label("Snap to grid").set_checked(true).set_on_change(&snapped, this);
    //   set.add(snap);
    class checkbox final : public control
    {
    public:
        // Called after a click changed the state.
        using change_fn = void (*)(void* user, checkbox& source) noexcept;

        // Text and names are UTF-8 and have to outlive the checkbox.
        checkbox& set_label(const char* label) noexcept { m_label = label; return *this; }
        checkbox& set_style(checkbox_style style) noexcept { m_style = style; return *this; }
        checkbox& set_state(check_state state) noexcept { m_state = state; m_pos = position(); return *this; }
        checkbox& set_checked(bool checked) noexcept { return set_state(checked ? check_state::checked : check_state::unchecked); }
        // A tri-state checkbox goes unchecked, checked, mixed, unchecked and so on. The others only flip.
        checkbox& set_tristate(bool tristate) noexcept { m_tristate = tristate; return *this; }
        // Name then value: the label goes to the left of the box unless this is turned off.
        checkbox& set_label_left(bool left) noexcept { m_label_left = left; return *this; }
        // What it is called when it has no label, such as in a status line.
        checkbox& set_name(const char* name) noexcept { m_name = name; return *this; }
        checkbox& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        checkbox& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] const char* label() const noexcept { return m_label; }
        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_label; }
        [[nodiscard]] checkbox_style style() const noexcept { return m_style; }
        [[nodiscard]] check_state state() const noexcept { return m_state; }
        [[nodiscard]] bool checked() const noexcept { return m_state == check_state::checked; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool animating() const noexcept override { return m_pos != position(); }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        void on_click() noexcept override;

    private:
        // Where the knob of a switch belongs for the state: 0 at the off end, 1 at the on end.
        [[nodiscard]] float position() const noexcept { return m_state == check_state::checked ? 1.0f : m_state == check_state::mixed ? 0.5f : 0.0f; }

        const char* m_label{nullptr};
        const char* m_name{nullptr};
        checkbox_style m_style{checkbox_style::box};
        check_state m_state{check_state::unchecked};
        bool m_tristate{false};
        bool m_label_left{true};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};
        // Where the knob is now, and when it was last moved. Only a switch slides, a box jumps.
        float m_pos{0.0f};
        std::uint64_t m_stamp{0};
    };
}
