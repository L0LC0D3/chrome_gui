#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    class radio;

    // The radios that choose between them: one is selected at a time. The group owns the choice, so selecting a radio
    // clears the one before it. The radios belong to the panel and have to outlive the group.
    //
    //   radio_group quality;
    //   quality.set_name("Quality").set_on_change(&picked, this);
    //   quality.add(low.set_label("Low"));
    //   quality.add(high.set_label("High"));
    //   quality.select(high);
    class radio_group
    {
    public:
        static constexpr std::uint32_t k_max_radios = 16;

        // Called after a click moved the selection. Selecting from code does not call it.
        using change_fn = void (*)(void* user, radio_group& source) noexcept;

        // false when the group is full.
        [[nodiscard]] bool add(radio& item) noexcept;
        // Makes the radio the selected one, and clears the others. It has to be in the group.
        // From code the dots jump. A click makes them grow and shrink.
        void select(radio& item, bool animate = false) noexcept;
        void select_index(std::uint32_t index) noexcept;
        void clear() noexcept;

        radio_group& set_name(const char* name) noexcept { m_name = name; return *this; }
        radio_group& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] const char* name() const noexcept { return m_name; }
        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        // The selected radio, or null when none is.
        [[nodiscard]] radio* selected() const noexcept { return m_selected; }
        // Its place in the group, or k_none.
        [[nodiscard]] std::uint32_t selected_index() const noexcept;

        static constexpr std::uint32_t k_none = 0xFFFFFFFFu;

    private:
        friend class radio;

        // A click: select, and tell the handler if that changed anything.
        void choose(radio& item) noexcept;

        radio* m_items[k_max_radios]{};
        std::uint32_t m_count{0};
        radio* m_selected{nullptr};
        const char* m_name{nullptr};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};
    };

    // A round button that is on or off, with an optional label. Radios in one radio_group are chosen between. A radio
    // that is on is only turned off by selecting another one, never by clicking it again.
    class radio final : public control
    {
    public:
        // Text and names are UTF-8 and have to outlive the radio.
        radio& set_label(const char* label) noexcept { m_label = label; return *this; }
        // Name then value: the label goes to the left of the circle unless this is turned off.
        radio& set_label_left(bool left) noexcept { m_label_left = left; return *this; }
        // What it is called when it has no label, such as in a status line.
        radio& set_name(const char* name) noexcept { m_name = name; return *this; }
        radio& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        // A number the program can tell the choices apart by.
        radio& set_value(std::int32_t value) noexcept { m_value = value; return *this; }

        [[nodiscard]] const char* label() const noexcept { return m_label; }
        [[nodiscard]] const char* name() const noexcept { return m_name != nullptr ? m_name : m_label; }
        [[nodiscard]] std::int32_t value() const noexcept { return m_value; }
        [[nodiscard]] bool selected() const noexcept { return m_selected; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool animating() const noexcept override { return m_pos != (m_selected ? 1.0f : 0.0f); }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        void on_click() noexcept override;

    private:
        friend class radio_group;

        // The group sets this, and the dot grows or shrinks to match.
        void set_selected(bool selected, bool animate) noexcept
        {
            m_selected = selected;

            if (!animate)
                m_pos = selected ? 1.0f : 0.0f;
        }

        void set_group(radio_group* group) noexcept { m_group = group; }

        const char* m_label{nullptr};
        const char* m_name{nullptr};
        radio_group* m_group{nullptr};
        std::int32_t m_value{0};
        bool m_selected{false};
        bool m_label_left{true};
        // How big the dot is now, 0 to 1, and when it was last moved.
        float m_pos{0.0f};
        std::uint64_t m_stamp{0};
    };
}
