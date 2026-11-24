#pragma once

#include <chrome/controls/core/icon_kind.hpp>
#include <chrome/dock.hpp>

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace chrome
{
    class dock_host;

    // What a drop does with the thing dragged.
    enum class drop_effect : std::uint8_t
    {
        // Nothing: the place does not take it.
        none,
        copy,
        move,
        link
    };

    // The effects a source allows, as a mask.
    namespace effects
    {
        constexpr std::uint8_t copy = 1;
        constexpr std::uint8_t move = 2;
        constexpr std::uint8_t link = 4;
        constexpr std::uint8_t all = copy | move | link;
    }

    // Paints the picture that follows the pointer, in place of the default one. dc is a Win32 HDC with the UI font selected,
    // area is the whole window of the picture, and effect is what a drop would do where the pointer is now.
    using ghost_fn = void (*)(void* user, void* dc, const dock_rect& area, drop_effect effect) noexcept;
    // Told how a drag ended. effect is what the drop did, or none when it was cancelled or nothing took it. A source that
    // gave a move removes the thing here.
    using drag_done_fn = void (*)(void* user, drop_effect effect) noexcept;

    // A four letter tag that tells the kinds of things dragged apart, such as kind_id("ASST") for an asset. A source and the
    // places that take its drops agree on one.
    [[nodiscard]] constexpr std::uint32_t kind_id(const char (&tag)[5]) noexcept
    {
        return static_cast<std::uint32_t>(static_cast<unsigned char>(tag[0])) | (static_cast<std::uint32_t>(static_cast<unsigned char>(tag[1])) << 8) |
            (static_cast<std::uint32_t>(static_cast<unsigned char>(tag[2])) << 16) | (static_cast<std::uint32_t>(static_cast<unsigned char>(tag[3])) << 24);
    }

    // The kind of a drag that comes from outside the program, such as files from Explorer. Its payload has no data, and files lists
    // their paths.
    constexpr std::uint32_t k_files_kind = kind_id("FILE");

    // What is being dragged: a kind, up to 256 bytes of data that the payload keeps, a label and a picture for the ghost that
    // follows the pointer, and what a drop may do. Build one on the stack and give it to control::begin_drag.
    //
    //   drag_payload payload;
    //   payload.set_data(kind_id("ASST"), asset_ref{id}).set_label("rock.fbx").set_icon(icon_kind::stack).set_count(3);
    //   payload.set_allowed(effects::copy | effects::move).set_on_done(&dropped, this);
    //   begin_drag(payload);
    struct drag_payload
    {
        static constexpr std::uint32_t k_data_size = 256;
        static constexpr std::uint32_t k_label_size = 64;

        std::uint32_t kind{0};
        std::uint32_t size{0};
        alignas(8) char data[k_data_size]{};
        char label[k_label_size]{};
        icon_kind icon{icon_kind::none};
        // The colour of the ghost's picture. 0 is the accent colour.
        std::uint32_t colour{0};
        // How many things are in the drag. More than one shows as a badge.
        std::uint32_t count{1};
        std::uint8_t allowed{effects::copy | effects::move};
        drag_done_fn done{nullptr};
        void* done_user{nullptr};
        // A ghost of the source's own, with its size in pixels. Without a painter the ghost is the label, picture and badge.
        ghost_fn ghost{nullptr};
        void* ghost_user{nullptr};
        std::int32_t ghost_width{0};
        std::int32_t ghost_height{0};
        // For a drag of files from outside: their full paths one after another, each ended by a zero, and the list by one more.
        // The host keeps the list until the drag is over, so a drop may read it but not keep the pointer.
        const wchar_t* files{nullptr};

        // Copies a value into the payload, and sets the kind. The value has to be plain data of at most k_data_size bytes. false when it is not.
        template <typename T>
        bool set_data(std::uint32_t tag, const T& value) noexcept
        {
            static_assert(std::is_trivially_copyable_v<T>, "A drag payload carries plain data");

            if (sizeof(T) > k_data_size)
                return false;

            kind = tag;
            size = static_cast<std::uint32_t>(sizeof(T));
            std::memcpy(data, &value, sizeof(T));
            return true;
        }

        drag_payload& set_kind(std::uint32_t tag) noexcept { kind = tag; return *this; }
        // The value put in with set_data, or null when the payload has none of that type.
        template <typename T>
        [[nodiscard]] const T* get() const noexcept
        {
            return size == sizeof(T) ? reinterpret_cast<const T*>(data) : nullptr;
        }

        drag_payload& set_label(const char* text) noexcept
        {
            std::size_t i = 0;

            for (; text != nullptr && text[i] != '\0' && i + 1 < k_label_size; ++i)
                label[i] = text[i];

            label[i] = '\0';
            return *this;
        }

        drag_payload& set_icon(icon_kind picture) noexcept { icon = picture; return *this; }
        drag_payload& set_colour(std::uint32_t rgb) noexcept { colour = rgb; return *this; }
        drag_payload& set_count(std::uint32_t things) noexcept { count = things; return *this; }
        drag_payload& set_allowed(std::uint8_t mask) noexcept { allowed = mask; return *this; }
        drag_payload& set_on_done(drag_done_fn handler, void* user) noexcept { done = handler; done_user = user; return *this; }
        drag_payload& set_ghost(ghost_fn painter, void* user, std::int32_t width, std::int32_t height) noexcept
        {
            ghost = painter;
            ghost_user = user;
            ghost_width = width;
            ghost_height = height;
            return *this;
        }

        [[nodiscard]] bool allows(drop_effect effect) const noexcept
        {
            return effect == drop_effect::copy ? (allowed & effects::copy) != 0 : effect == drop_effect::move ? (allowed & effects::move) != 0
                : effect == drop_effect::link && (allowed & effects::link) != 0;
        }
    };

    // The pointer over a place that may take the drop. x and y are in the coordinates of whoever is asked: a panel, or the
    // controls of one. wanted is what the keys ask for (Ctrl copy, Shift move, Alt link, otherwise the source's first choice),
    // always one the source allows. A place answers with the effect it will have, which may differ from wanted.
    struct drag_event
    {
        const drag_payload* payload{nullptr};
        std::int32_t x{0};
        std::int32_t y{0};
        bool ctrl{false};
        bool shift{false};
        bool alt{false};
        drop_effect wanted{drop_effect::copy};

        [[nodiscard]] bool is(std::uint32_t tag) const noexcept { return payload != nullptr && payload->kind == tag; }
    };

    namespace drag_drop
    {
        // The effect the keys ask for among those a payload allows. none when it allows none.
        [[nodiscard]] drop_effect wanted_effect(const drag_payload& payload, bool ctrl, bool shift, bool alt) noexcept;
        // How far the pointer goes with a button held before a press becomes a drag, in pixels.
        [[nodiscard]] std::int32_t threshold() noexcept;
        [[nodiscard]] bool exceeds_threshold(std::int32_t dx, std::int32_t dy) noexcept;

        // The host the session runs in. Set once by the dock host, which owns the ghost and the capture.
        void install(dock_host* host) noexcept;
        // A drag is in progress.
        [[nodiscard]] bool active() noexcept;
        // Starts a drag, which follows the pointer until the button comes up. The mouse has to be held down on a panel. false when
        // one is already going, or there is no host. Controls call control::begin_drag instead.
        bool begin(const drag_payload& payload) noexcept;
        // Ends the drag with nothing dropped.
        void cancel() noexcept;
    }
}
