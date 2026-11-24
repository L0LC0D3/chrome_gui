#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // A box that takes things dragged to it: a trash can, a slot for an asset, a place to drop files. It lists the kinds it
    // takes, and the effects it answers with, and it shows whether a drag over it would be taken and what it would do. When
    // something is dropped it calls the handler, and it can keep a line of text for what it holds.
    //
    //   drop_box slot;
    //   slot.set_name("Material").set_text("Drop a material here").add_kind(kind_id("ASST")).set_on_drop(&dropped, this);
    //   set.add(slot);
    class drop_box final : public control
    {
    public:
        static constexpr std::uint32_t k_max_kinds = 4;

        // Called after a drop was taken. effect is what the drop did.
        using drop_fn = void (*)(void* user, drop_box& source, const drag_event& event, drop_effect effect) noexcept;
        // Decides whether a drag over the zone is taken, beyond its kind. Optional.
        using accept_fn = bool (*)(void* user, drop_box& source, const drag_event& event) noexcept;

        // A kind of thing the zone takes. With none listed it takes every kind. false when the list is full.
        bool add_kind(std::uint32_t tag) noexcept;
        // The effects it answers with, from effects::. A drag that allows none of them is refused. All by default.
        drop_box& set_effects(std::uint8_t mask) noexcept { m_effects = mask; return *this; }
        // What shows while it is empty.
        drop_box& set_prompt(const char* text) noexcept { m_prompt = text; return *this; }
        // What it holds, UTF-8, which has to outlive the zone. Shown instead of the prompt. Null for empty.
        drop_box& set_text(const char* text) noexcept { m_text = text; invalidate(); return *this; }
        drop_box& set_icon(icon_kind icon) noexcept { m_icon = icon; return *this; }
        drop_box& set_width(std::int32_t width) noexcept { m_width = width; return *this; }
        drop_box& set_height(std::int32_t height) noexcept { m_height = height; return *this; }
        drop_box& set_name(const char* name) noexcept { m_name = name; return *this; }
        drop_box& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        drop_box& set_on_drop(drop_fn handler, void* user) noexcept { m_drop = handler; m_drop_user = user; return *this; }
        drop_box& set_on_accept(accept_fn handler, void* user) noexcept { m_accept = handler; m_accept_user = user; return *this; }

        [[nodiscard]] const char* name() const noexcept { return m_name; }
        [[nodiscard]] const char* text() const noexcept { return m_text; }
        // A drag is over the zone, and it would be taken.
        [[nodiscard]] bool receiving() const noexcept { return m_effect != drop_effect::none; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;
        [[nodiscard]] bool is_drop_target() const noexcept override { return true; }

    protected:
        drop_effect on_drag_over(const drag_event& event) noexcept override;
        void on_drag_leave() noexcept override;
        drop_effect on_drop(const drag_event& event) noexcept override;

    private:
        // What the zone would do with a drag, or none.
        [[nodiscard]] drop_effect judge(const drag_event& event) noexcept;

        std::uint32_t m_kinds[k_max_kinds]{};
        std::uint32_t m_kind_count{0};
        std::uint8_t m_effects{effects::all};
        const char* m_prompt{"Drop here"};
        const char* m_text{nullptr};
        const char* m_name{nullptr};
        icon_kind m_icon{icon_kind::none};
        std::int32_t m_width{200};
        std::int32_t m_height{64};
        drop_fn m_drop{nullptr};
        void* m_drop_user{nullptr};
        accept_fn m_accept{nullptr};
        void* m_accept_user{nullptr};
        // A drag is over the zone: what it would do, which is none while the zone refuses it.
        bool m_over{false};
        drop_effect m_effect{drop_effect::none};
    };
}
