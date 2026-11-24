#pragma once

#include <chrome/controls/core/control.hpp>

#include <cstdint>

namespace chrome
{
    // The shape of the mark in a count toggle.
    enum class mark_kind : std::uint8_t
    {
        disc,
        triangle,
        // A disc with a cross in it.
        cross_disc
    };

    // A small toggle for a toolbar made of a coloured mark and a number: how many of something there are, which is shown
    // or not as the toggle is on or off. It is how a console tells its errors, warnings and messages apart.
    //
    //   count_toggle errors;
    //   errors.set_mark(mark_kind::cross_disc).set_colour(0xE5534B).set_checked(true).set_on_change(&filtered, this);
    //   errors.set_count(12);
    class count_toggle final : public control
    {
    public:
        using change_fn = void (*)(void* user, count_toggle& source) noexcept;

        count_toggle& set_mark(mark_kind mark) noexcept { m_mark = mark; return *this; }
        // The colour of the mark when the toggle is on.
        count_toggle& set_colour(std::uint32_t colour) noexcept { m_colour = colour; return *this; }
        count_toggle& set_count(std::uint32_t count) noexcept { m_count = count; return *this; }
        count_toggle& set_checked(bool checked) noexcept { m_checked = checked; return *this; }
        count_toggle& set_name(const char* name) noexcept { m_name = name; return *this; }
        count_toggle& set_tooltip(const char* text) noexcept { control::set_tooltip(text); return *this; }
        count_toggle& set_on_change(change_fn handler, void* user) noexcept { m_handler = handler; m_user = user; return *this; }

        [[nodiscard]] bool checked() const noexcept { return m_checked; }
        [[nodiscard]] std::uint32_t count() const noexcept { return m_count; }
        [[nodiscard]] const char* name() const noexcept { return m_name; }

        [[nodiscard]] control_size preferred_size(void* dc) const noexcept override;
        void paint(void* dc) const noexcept override;

    protected:
        void on_click() noexcept override;

    private:
        mark_kind m_mark{mark_kind::disc};
        std::uint32_t m_colour{0x6CA6E0};
        std::uint32_t m_count{0};
        bool m_checked{true};
        const char* m_name{nullptr};
        change_fn m_handler{nullptr};
        void* m_user{nullptr};
    };
}
