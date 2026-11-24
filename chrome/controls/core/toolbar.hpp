#pragma once

#include <chrome/controls/core/button.hpp>

#include <cstdint>

namespace chrome
{
    // A row of flat icon buttons, toggles and lines between groups, with a place to push the last group to the right.
    // It owns the buttons, so each has a tooltip, a hover and a disabled look like any button. It is not a control
    // itself: the panel that shows it attaches the buttons to its control set, calls arrange when its size changes and
    // paint_separators before it paints the set.
    //
    //   toolbar bar;
    //   bar.set_on_action(&acted, this);
    //   bar.add_button(icon_kind::plus, k_new, "New scene");
    //   bar.add_separator();
    //   bar.add_toggle(icon_kind::play, k_play, "Play").set_checked(false);
    //   bar.add_spacer();
    //   bar.add_button(icon_kind::panel, k_panels, "Panels");
    //   bar.attach(set);
    class toolbar
    {
    public:
        static constexpr std::uint32_t k_max_items = 32;

        // Called after a button was clicked, with the id it was added with. A toggle has already changed by then.
        using action_fn = void (*)(void* user, toolbar& source, std::int32_t id) noexcept;

        // New buttons go at the end, or the last one again when the bar is full. Texts are UTF-8 and have to outlive the
        // bar. A label makes the button wider, with the icon in front of it.
        button& add_button(icon_kind icon, std::int32_t id, const char* tooltip, const char* label = nullptr) noexcept;
        button& add_toggle(icon_kind icon, std::int32_t id, const char* tooltip, const char* label = nullptr) noexcept;
        // A thin line between two groups of buttons.
        void add_separator() noexcept;
        // Everything added after this sits at the right edge.
        void add_spacer() noexcept;
        toolbar& set_on_action(action_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        // The button with an id, or null.
        [[nodiscard]] button* find(std::int32_t id) noexcept;
        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }

        // Takes every button and line away, so the bar can be filled again. The buttons that were attached to a control set stay
        // in it, empty and out of sight, so a bar that is refilled is attached with attach_all, once.
        void clear() noexcept;
        // Puts the buttons into a control set. false when the set is full.
        [[nodiscard]] bool attach(control_set& set) noexcept;
        // Puts every place of the bar into a control set, filled or not, so buttons added later are in it already.
        [[nodiscard]] bool attach_all(control_set& set) noexcept;
        // Places the buttons in a row, centred vertically, in an area of the panel. dc has the UI font selected.
        void arrange(void* dc, const dock_rect& area) noexcept;
        // Draws the lines between groups.
        void paint_separators(void* dc) const noexcept;

    private:
        enum class kind : std::uint8_t
        {
            action,
            separator,
            spacer
        };

        struct item
        {
            button face{};
            toolbar* owner{nullptr};
            std::int32_t id{0};
            kind what{kind::action};
            dock_rect rule{};
        };

        item& next_item() noexcept;
        static void clicked(void* user, button& source) noexcept;

        item m_items[k_max_items]{};
        std::uint32_t m_count{0};
        action_fn m_handler{nullptr};
        void* m_user{nullptr};
    };
}
