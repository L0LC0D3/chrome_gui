#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    enum class group_style : std::uint8_t
    {
        // A bold title with a line under it, as the headers of an inspector. Its controls are not indented.
        header,
        // A bar on top of a frame that the panel draws around the controls of the group, which sit inside it.
        box
    };

    // The title of a group of controls, which can be folded away. It is a control of its own, and the controls that
    // follow it in the panel, up to the next group or the end of the section, belong to it. It only keeps the state and
    // draws the bar: the panel hides the controls of a folded group and lays out the rest, and draws the frame of a box.
    //
    //   group transform;
    //   transform.set_title("Transform").set_style(group_style::box).set_on_toggle(&toggled, this);
    //   set.add(transform);
    //   set.add(position);              // inside the group
    class group final : public control
    {
    public:
        // Called after a click or a key folded or unfolded the group. Setting it from code does not call it.
        using toggle_fn = void (*)(void* user, group& source) noexcept;

        // Text is UTF-8 and has to outlive the group.
        group& set_title(const char* title) noexcept { m_title = title; return *this; }
        group& set_style(group_style style) noexcept { m_style = style; return *this; }
        // A group that cannot be folded has no arrow and stays open.
        group& set_collapsible(bool collapsible) noexcept { m_collapsible = collapsible; return *this; }
        // The arrow turns when animate is set, and jumps otherwise.
        group& set_expanded(bool expanded, bool animate = false) noexcept;
        group& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        group& set_on_toggle(toggle_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] const char* title() const noexcept { return m_title; }
        [[nodiscard]] group_style style() const noexcept { return m_style; }
        [[nodiscard]] bool collapsible() const noexcept { return m_collapsible; }
        [[nodiscard]] bool expanded() const noexcept { return m_expanded; }
        // How much of its controls show, 0 for folded to 1 for open. It follows expanded() smoothly, so the panel lays out
        // the controls at this fraction of their height while the group folds or unfolds.
        [[nodiscard]] float reveal() const noexcept { return m_pos; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return true; }
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool animating() const noexcept override { return m_pos != (m_expanded ? 1.0f : 0.0f); }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        void on_click() noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return m_collapsible; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;

    private:
        void toggle(bool expanded) noexcept;

        const char* m_title{nullptr};
        group_style m_style{group_style::header};
        bool m_collapsible{true};
        bool m_expanded{true};
        // How far the arrow has turned, 0 for folded to 1 for open, and when it was last moved.
        float m_pos{1.0f};
        std::uint64_t m_stamp{0};
        toggle_fn m_handler{nullptr};
        void* m_user{nullptr};
    };
}
