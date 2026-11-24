#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // One part of a proportion_bar. The bar copies the texts, so they need not outlive the call.
    struct bar_segment
    {
        // Names the part from one update to the next, so its width eases instead of jumping and a selection follows it.
        std::uint32_t id{0};
        const char* label{nullptr};
        // In the unit of the bar. The width is its share of the whole.
        float value{0.0f};
        // 0xRRGGBB, or proportion_bar::k_auto_colour to take a colour from the id.
        std::uint32_t colour{0xFFFFFFFFu};
        // Waiting, not working: drawn hatched and dim.
        bool idle{false};
        // A second line of the tooltip. May be null.
        const char* detail{nullptr};
    };

    // A bar cut into parts whose widths are shares of a whole, such as where a frame went: each part is a zone, as wide as the
    // time it took. The whole is the sum of the parts, or a scale given with set_scale, and a budget draws a line where a frame is
    // meant to end, with the part past it marked. Parts narrower than half a percent that sit together are drawn as one. Widths
    // ease toward new values, and the bar asks the panel for frames while they do.
    //
    // A click selects a part and a double click, or Enter, activates it, which is how a view goes into it. Left, Right, Home and
    // End move the selection. The tooltip names the part under the mouse, its time and share.
    //
    //   proportion_bar bar;
    //   bar.set_caption("SIM", "7.9 ms").set_unit("ms").set_scale(16.6f).set_budget(16.6f);
    //   const bar_segment parts[] = {{1, "input", 0.4f}, {2, "sim", 4.0f}, {3, "sleep", 5.0f, 0xFFFFFFFFu, true}};
    //   bar.set_segments(parts, 3);
    //   set.add(bar);
    class proportion_bar final : public control
    {
    public:
        static constexpr std::uint32_t k_max_segments = 64;
        static constexpr std::uint32_t k_auto_colour = 0xFFFFFFFFu;
        // The id of the part made of several small ones.
        static constexpr std::uint32_t k_other = 0xFFFFFFFFu;

        // Called when the selection changes by the user, and when a part is activated. id is that of the part.
        using select_fn = void (*)(void* user, proportion_bar& source, std::uint32_t id) noexcept;

        proportion_bar& set_name(const char* name) noexcept { m_name = name; return *this; }
        // Text before the bar: a name on the left and a value on the right, both copied. Null removes the caption.
        proportion_bar& set_caption(const char* name, const char* value) noexcept;
        // The width of the caption at 96 dpi.
        proportion_bar& set_caption_width(std::int32_t width) noexcept { m_caption_width = width; return *this; }
        // Draws the caption name in the accent colour, to mark the bar a view is following.
        proportion_bar& set_emphasis(bool on) noexcept
        {
            if (m_emphasis != on)
            {
                m_emphasis = on;
                invalidate();
            }

            return *this;
        }
        // The height of the control at 96 dpi.
        proportion_bar& set_height(std::int32_t height) noexcept { m_height = height; return *this; }
        // What the values are in, shown after them in the tooltip.
        proportion_bar& set_unit(const char* unit) noexcept;
        // The value that fills the bar, or 0 for the sum of the parts. When the parts add up to more, the bar shows them all and
        // the budget line moves in from the end.
        proportion_bar& set_scale(float scale) noexcept { m_scale = scale; invalidate(); return *this; }
        // Where the line goes, in the unit of the bar. 0 for none.
        proportion_bar& set_budget(float budget) noexcept { m_budget = budget; invalidate(); return *this; }
        // What the tooltip says when the mouse is not on a part. Has to outlive the bar.
        proportion_bar& set_help(const char* text) noexcept;
        proportion_bar& set_on_select(select_fn fn, void* user) noexcept { m_on_select = fn; m_on_select_user = user; return *this; }
        proportion_bar& set_on_activate(select_fn fn, void* user) noexcept { m_on_activate = fn; m_on_activate_user = user; return *this; }

        // Replaces the parts. Parts that keep their id keep their place in the animation. Past k_max_segments the rest are dropped.
        void set_segments(const bar_segment* segments, std::uint32_t count) noexcept;
        [[nodiscard]] std::uint32_t segment_count() const noexcept { return m_count; }

        // The selected part, which may be none. Setting it does not call the handler.
        void set_selected(std::uint32_t id) noexcept;
        void clear_selection() noexcept;
        [[nodiscard]] bool has_selection() const noexcept { return m_has_selection; }
        [[nodiscard]] std::uint32_t selected() const noexcept { return m_selected; }

        [[nodiscard]] const char* name() const noexcept { return m_name; }

        // A colour for a part from a number such as the hash of its name, the same every time, medium bright so that dark and
        // light text both read on the dark and the light theme.
        [[nodiscard]] static std::uint32_t segment_colour(std::uint32_t seed) noexcept;

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        [[nodiscard]] control_size preferred_size_in(void* dc, std::int32_t max_width) const noexcept override;
        [[nodiscard]] bool fills_row() const noexcept override { return true; }
        [[nodiscard]] dock_rect tooltip_area() const noexcept override;
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool animating() const noexcept override { return m_moving; }
        bool animate(std::uint64_t now_ms) noexcept override;

    protected:
        void on_press(std::int32_t x, std::int32_t y) noexcept override;
        void on_double_click(std::int32_t x, std::int32_t y) noexcept override;
        bool on_move(std::int32_t x, std::int32_t y) noexcept override;
        void on_leave() noexcept override;
        [[nodiscard]] bool focusable() const noexcept override { return true; }
        [[nodiscard]] bool wants_focus_ring() const noexcept override { return true; }
        bool on_key(std::uint32_t key, bool ctrl, bool shift, bool alt) noexcept override;

    private:
        struct item
        {
            std::uint32_t id{0};
            char label[40]{};
            char detail[96]{};
            float value{0.0f};
            // What is drawn while the width eases toward value.
            float shown{0.0f};
            std::uint32_t colour{0};
            bool idle{false};
        };

        // What the bar draws: one part, or a run of small ones.
        struct slot
        {
            std::int32_t x{0};
            std::int32_t width{0};
            std::uint32_t first{0};
            std::uint32_t count{0};
        };

        [[nodiscard]] dock_rect track_area() const noexcept;
        // Cuts the parts into slots for a track of this width, from the values being shown.
        void make_slots(std::int32_t track_x, std::int32_t track_width) const noexcept;
        [[nodiscard]] std::int32_t slot_at(std::int32_t x, std::int32_t y) const noexcept;
        [[nodiscard]] std::int32_t index_of(std::uint32_t id) const noexcept;
        void show_tooltip(std::int32_t slot_index) noexcept;
        void select_index(std::uint32_t index, bool by_user) noexcept;

        const char* m_name{nullptr};
        const char* m_help{nullptr};
        char m_caption_name[32]{};
        char m_caption_value[24]{};
        char m_unit[8]{'m', 's', '\0'};
        std::int32_t m_caption_width{120};
        std::int32_t m_height{28};
        float m_scale{0.0f};
        float m_budget{0.0f};
        bool m_emphasis{false};

        item m_items[k_max_segments]{};
        std::uint32_t m_count{0};
        // The sum of the target values, which the shares in the tooltip are of.
        float m_sum{0.0f};
        bool m_moving{false};
        std::uint64_t m_stamp{0};

        bool m_has_selection{false};
        std::uint32_t m_selected{0};
        std::int32_t m_hover{-1};

        mutable slot m_slots[k_max_segments]{};
        mutable std::uint32_t m_slot_count{0};
        mutable float m_full{0.0f};
        // The tooltip texts take turns, so a new one differs in address from the one on show and the host shows it.
        char m_tip[2][208]{};
        std::uint32_t m_tip_next{0};

        select_fn m_on_select{nullptr};
        void* m_on_select_user{nullptr};
        select_fn m_on_activate{nullptr};
        void* m_on_activate_user{nullptr};
    };
}
