// SPDX-License-Identifier: MIT
// NexKey - IBus input method engine.
//
// The engine is a small GObject wrapper around nexkey::Engine: key events are
// translated to Unicode code points, the preedit and the word selection window
// are published through the IBus engine API, and the configuration file is
// reloaded when it changes.

#include "nexkey/Engine.hpp"
#include "nexkey/Settings.hpp"
#include "nexkey/Types.hpp"

#include <ibus.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr auto kEngineName = "nexkey";
constexpr auto kBusName = "org.freedesktop.IBus.NexKey";
constexpr std::uint32_t kReleaseMask = 1U << 30U;
constexpr guint kModifierControl = 1U << 2U;
constexpr guint kModifierShift = 1U << 0U;
constexpr guint kModifierAlt = 1U << 3U;
constexpr guint kModifierSuper = 1U << 26U;

struct ToggleKeys {
    guint keyval{0};
    guint modifiers{0};

    [[nodiscard]] bool valid() const { return keyval != 0 || modifiers != 0; }
    /// A modifier only shortcut such as "Control+Shift".
    [[nodiscard]] bool modifier_only() const { return keyval == 0; }
};

/// State shared by every IBus engine object of NexKey.
struct Session {
    nexkey::Engine engine{nexkey::load_settings().options};
    nexkey::Settings settings{nexkey::load_settings()};
    ToggleKeys toggle{};
    std::filesystem::file_time_type config_time{};
    std::chrono::steady_clock::time_point last_check{};

    // Word selection window state.
    std::vector<nexkey::Candidate> candidates;
    int cursor{0};

    bool preedit_visible{false};

    void reload(bool force = false)
    {
        const auto now = std::chrono::steady_clock::now();
        if (!force && now - last_check < std::chrono::milliseconds(250)) {
            return;
        }
        last_check = now;

        const auto path = nexkey::default_config_path();
        std::error_code error;
        const auto modified = std::filesystem::last_write_time(path, error);
        if (!force && !error && modified == config_time) {
            return;
        }
        if (!error) {
            config_time = modified;
        }

        settings = nexkey::load_settings(path);
        engine.set_options(settings.options);
        toggle = parse_toggle(settings.toggle_key);
    }

    static ToggleKeys parse_toggle(std::string_view text)
    {
        ToggleKeys result;
        std::size_t start = 0;
        while (start <= text.size()) {
            const auto end = text.find('+', start);
            const auto part = text.substr(
                start, end == std::string_view::npos ? text.size() - start : end - start);
            if (!part.empty()) {
                if (is_modifier_name(part)) {
                    result.modifiers |= modifier_of(part);
                } else {
                    const std::string name(part);
                    if (ibus_key_event_from_string(name.c_str(), &result.keyval,
                                                   &result.modifiers) == FALSE) {
                        result.keyval = 0;
                    }
                }
            }
            if (end == std::string_view::npos) {
                break;
            }
            start = end + 1;
        }
        return result;
    }

    static bool is_modifier_name(std::string_view part)
    {
        return part == "Control" || part == "Ctrl" || part == "Shift" || part == "Alt" ||
               part == "Super" || part == "Mod4";
    }

    static guint modifier_of(std::string_view part)
    {
        if (part == "Control" || part == "Ctrl") {
            return kModifierControl;
        }
        if (part == "Shift") {
            return kModifierShift;
        }
        if (part == "Alt") {
            return kModifierAlt;
        }
        return kModifierSuper;
    }
};

} // namespace

// --- GObject boilerplate ----------------------------------------------------

typedef struct _NexKeyIbusEngine NexKeyIbusEngine;
typedef struct _NexKeyIbusEngineClass NexKeyIbusEngineClass;

struct _NexKeyIbusEngine {
    IBusEngine parent;
    Session* session;
};

struct _NexKeyIbusEngineClass {
    IBusEngineClass parent_class;
};

#define NEXKEY_TYPE_IBUS_ENGINE (nexkey_ibus_engine_get_type())
#define NEXKEY_IBUS_ENGINE(object)                                                    \
    (G_TYPE_CHECK_INSTANCE_CAST((object), NEXKEY_TYPE_IBUS_ENGINE, NexKeyIbusEngine))
#define NEXKEY_IS_IBUS_ENGINE(object)                                                 \
    (G_TYPE_CHECK_INSTANCE_TYPE((object), NEXKEY_TYPE_IBUS_ENGINE))

G_DEFINE_TYPE(NexKeyIbusEngine, nexkey_ibus_engine, IBUS_TYPE_ENGINE)

namespace {

// --- helpers ----------------------------------------------------------------

void commit_text(IBusEngine* engine, const std::string& text)
{
    if (text.empty()) {
        return;
    }
    ibus_engine_commit_text(engine, ibus_text_new_from_string(text.c_str()));
}

void update_preedit(IBusEngine* engine, Session& session)
{
    const std::string preedit(session.engine.preedit());
    if (preedit.empty()) {
        ibus_engine_update_preedit_text_with_mode(
            engine, ibus_text_new_from_string(""), 0, FALSE, IBUS_ENGINE_PREEDIT_CLEAR);
        session.preedit_visible = false;
        return;
    }
    auto* text = ibus_text_new_from_string(preedit.c_str());
    ibus_engine_update_preedit_text_with_mode(engine, text, ibus_text_get_length(text), TRUE,
                                              IBUS_ENGINE_PREEDIT_COMMIT);
    session.preedit_visible = true;
}

void update_lookup_table(IBusEngine* engine, Session& session)
{
    session.candidates = session.engine.candidates();
    if (session.candidates.size() < 2) {
        session.candidates.clear();
        session.cursor = 0;
        ibus_engine_hide_lookup_table(engine);
        return;
    }

    session.cursor = std::clamp(session.cursor, 0, static_cast<int>(session.candidates.size()) - 1);

    auto* table = ibus_lookup_table_new(
        static_cast<guint>(session.candidates.size()), 0, TRUE, FALSE);
    for (const auto& candidate : session.candidates) {
        ibus_lookup_table_append_candidate(
            table, ibus_text_new_from_string(candidate.text.c_str()));
    }
    ibus_lookup_table_set_cursor_pos(table, static_cast<guint>(session.cursor));
    ibus_engine_update_lookup_table(engine, table, TRUE);
    ibus_engine_show_lookup_table(engine);
}

void update_properties(IBusEngine* engine, const Session& session)
{
    auto* mode_item = ibus_property_new(
        "nexkey-mode", PROP_TYPE_RADIO,
        ibus_text_new_from_static_string(session.settings.enabled ? "Vietnamese (VI)"
                                                                  : "English (EN)"),
        nullptr, ibus_text_new_from_static_string("Switch between VI and EN"),
        TRUE, TRUE,
        session.settings.enabled ? PROP_STATE_CHECKED : PROP_STATE_UNCHECKED, nullptr);
    ibus_property_set_tooltip(mode_item, ibus_text_new_from_static_string(
                                            "Toggle with the configured shortcut"));

    auto* menu = ibus_prop_list_new();
    ibus_prop_list_append(menu, mode_item);

    auto* root = ibus_property_new(
        "nexkey", PROP_TYPE_MENU, ibus_text_new_from_static_string("NexKey"), nullptr,
        ibus_text_new_from_static_string("NexKey options"), TRUE, TRUE, PROP_STATE_UNCHECKED,
        menu);

    auto* all = ibus_prop_list_new();
    ibus_prop_list_append(all, root);
    ibus_engine_register_properties(engine, all);
}

/// Commit everything that is still being composed.
void commit_all(IBusEngine* engine, Session& session)
{
    const auto step = session.engine.flush();
    commit_text(engine, step.commit);
    session.candidates.clear();
    session.cursor = 0;
    ibus_engine_hide_lookup_table(engine);
    update_preedit(engine, session);
}

/// Returns true when the key picked a candidate.
bool select_candidate(IBusEngine* engine, Session& session, guint keyval, guint state)
{
    if (session.candidates.size() < 2) {
        return false;
    }
    if (keyval < IBUS_KEY_1 || keyval > IBUS_KEY_9) {
        return false;
    }

    const bool digits_are_text = session.settings.options.method == nexkey::Method::Vni ||
                                 session.settings.options.method == nexkey::Method::Viqr;
    const bool with_ctrl = (state & kModifierControl) != 0;
    if (digits_are_text && !with_ctrl) {
        return false;
    }

    const auto index = static_cast<std::size_t>(keyval - IBUS_KEY_1);
    if (index >= session.candidates.size()) {
        return false;
    }

    const auto text = session.candidates[index].text;
    session.engine.discard();
    session.candidates.clear();
    session.cursor = 0;
    ibus_engine_hide_lookup_table(engine);
    commit_text(engine, text);
    update_preedit(engine, session);
    return true;
}

/// True for the modifier key symbols themselves, used to detect a
/// modifier only shortcut such as "Control+Shift".
bool is_modifier_keyval(guint keyval)
{
    switch (keyval) {
    case IBUS_KEY_Shift_L:
    case IBUS_KEY_Shift_R:
    case IBUS_KEY_Control_L:
    case IBUS_KEY_Control_R:
    case IBUS_KEY_Alt_L:
    case IBUS_KEY_Alt_R:
    case IBUS_KEY_Meta_L:
    case IBUS_KEY_Meta_R:
    case IBUS_KEY_Super_L:
    case IBUS_KEY_Super_R:
    case IBUS_KEY_Hyper_L:
    case IBUS_KEY_Hyper_R:
    case IBUS_KEY_ISO_Level3_Shift:
    case IBUS_KEY_ISO_Level5_Shift:
        return true;
    default:
        return false;
    }
}

bool toggle_matches(const Session& session, guint keyval, guint state)
{
    const auto& toggle = session.toggle;
    if (!toggle.valid()) {
        return false;
    }
    if (toggle.modifier_only()) {
        // Fired by the key press of the last modifier, e.g. Shift while
        // Control is held for "Control+Shift".
        return (state & toggle.modifiers) == toggle.modifiers;
    }
    return keyval == toggle.keyval && (state & toggle.modifiers) == toggle.modifiers;
}

void apply_mode(IBusEngine* engine, Session& session)
{
    session.engine.set_options(session.settings.options);
    session.engine.discard();
    session.candidates.clear();
    ibus_engine_hide_lookup_table(engine);
    update_preedit(engine, session);
    update_properties(engine, session);
    ibus_engine_update_auxiliary_text(
        engine,
        ibus_text_new_from_static_string(session.settings.enabled ? "VI" : "EN"), TRUE);
}

// --- vfuncs -----------------------------------------------------------------

gboolean on_process_key_event(IBusEngine* engine, guint keyval, guint /*keycode*/, guint state)
{
    auto* self = NEXKEY_IBUS_ENGINE(engine);
    auto& session = *self->session;
    session.reload();

    if ((state & kReleaseMask) != 0) {
        return FALSE;
    }

    // The toggle shortcut is handled before anything else. A modifier only
    // shortcut must only react to the modifier key itself, otherwise every key
    // typed while holding the modifiers would switch the mode.
    const bool modifier_shortcut = session.toggle.modifier_only();
    if (toggle_matches(session, keyval, state) && (!modifier_shortcut || is_modifier_keyval(keyval))) {
        session.settings.enabled = !session.settings.enabled;
        static_cast<void>(nexkey::save_settings(session.settings));
        apply_mode(engine, session);
        return TRUE;
    }

    if (select_candidate(engine, session, keyval, state)) {
        return TRUE;
    }

    if (!session.settings.enabled) {
        commit_all(engine, session);
        return FALSE;
    }

    if ((state & (kModifierControl | kModifierAlt | kModifierSuper)) != 0) {
        commit_all(engine, session);
        return FALSE;
    }

    if (keyval == IBUS_KEY_BackSpace) {
        const auto step = session.engine.backspace();
        if (!step.handled) {
            commit_all(engine, session);
            return FALSE;
        }
        update_preedit(engine, session);
        update_lookup_table(engine, session);
        return TRUE;
    }

    if (keyval == IBUS_KEY_Return || keyval == IBUS_KEY_KP_Enter || keyval == IBUS_KEY_Escape ||
        keyval == IBUS_KEY_Tab || keyval == IBUS_KEY_Up || keyval == IBUS_KEY_Down) {
        commit_all(engine, session);
        return FALSE;
    }

    gunichar code_point = ibus_keyval_to_unicode(keyval);
    if (code_point == 0) {
        commit_all(engine, session);
        return FALSE;
    }

    const auto step = session.engine.type(static_cast<char32_t>(code_point));
    if (!step.handled) {
        commit_all(engine, session);
        return FALSE;
    }
    commit_text(engine, step.commit);
    update_preedit(engine, session);
    update_lookup_table(engine, session);
    return TRUE;
}

void on_focus_in(IBusEngine* engine)
{
    auto& session = *NEXKEY_IBUS_ENGINE(engine)->session;
    session.reload(true);
    session.engine.discard();
    update_properties(engine, session);
    ibus_engine_update_auxiliary_text(
        engine,
        ibus_text_new_from_static_string(session.settings.enabled ? "VI" : "EN"), TRUE);
}

void on_focus_out(IBusEngine* engine)
{
    auto& session = *NEXKEY_IBUS_ENGINE(engine)->session;
    commit_all(engine, session);
    session.engine.discard();
}

void on_reset(IBusEngine* engine)
{
    auto& session = *NEXKEY_IBUS_ENGINE(engine)->session;
    session.engine.discard();
    session.candidates.clear();
    ibus_engine_hide_lookup_table(engine);
    update_preedit(engine, session);
}

void on_enable(IBusEngine* engine)
{
    auto& session = *NEXKEY_IBUS_ENGINE(engine)->session;
    apply_mode(engine, session);
}

void on_disable(IBusEngine* engine)
{
    on_focus_out(engine);
}

void on_property_activate(IBusEngine* engine, const gchar* prop_name, guint /*prop_state*/)
{
    auto& session = *NEXKEY_IBUS_ENGINE(engine)->session;
    if (g_strcmp0(prop_name, "nexkey-mode") != 0) {
        return;
    }
    session.settings.enabled = !session.settings.enabled;
    static_cast<void>(nexkey::save_settings(session.settings));
    apply_mode(engine, session);
}

void on_candidate_clicked(IBusEngine* engine, guint index, guint /*button*/, guint /*state*/)
{
    auto& session = *NEXKEY_IBUS_ENGINE(engine)->session;
    if (index >= session.candidates.size()) {
        return;
    }
    const auto text = session.candidates[index].text;
    session.engine.discard();
    session.candidates.clear();
    ibus_engine_hide_lookup_table(engine);
    commit_text(engine, text);
    update_preedit(engine, session);
}

void on_cursor_up(IBusEngine* engine)
{
    auto& session = *NEXKEY_IBUS_ENGINE(engine)->session;
    if (session.candidates.size() < 2) {
        return;
    }
    session.cursor = session.cursor == 0 ? static_cast<int>(session.candidates.size()) - 1
                                         : session.cursor - 1;
    update_lookup_table(engine, session);
}

void on_cursor_down(IBusEngine* engine)
{
    auto& session = *NEXKEY_IBUS_ENGINE(engine)->session;
    if (session.candidates.size() < 2) {
        return;
    }
    session.cursor = (session.cursor + 1) % static_cast<int>(session.candidates.size());
    update_lookup_table(engine, session);
}

void on_finalize(GObject* object)
{
    auto* self = NEXKEY_IBUS_ENGINE(object);
    delete self->session;
    self->session = nullptr;
    G_OBJECT_CLASS(nexkey_ibus_engine_parent_class)->finalize(object);
}

} // namespace

static void nexkey_ibus_engine_init(NexKeyIbusEngine* self)
{
    self->session = new Session();
    self->session->reload(true);
}

static void nexkey_ibus_engine_class_init(NexKeyIbusEngineClass* klass)
{
    auto* object_class = G_OBJECT_CLASS(klass);
    auto* engine_class = IBUS_ENGINE_CLASS(klass);

    object_class->finalize = on_finalize;

    engine_class->process_key_event = on_process_key_event;
    engine_class->focus_in = on_focus_in;
    engine_class->focus_out = on_focus_out;
    engine_class->reset = on_reset;
    engine_class->enable = on_enable;
    engine_class->disable = on_disable;
    engine_class->property_activate = on_property_activate;
    engine_class->candidate_clicked = on_candidate_clicked;
    engine_class->cursor_up = on_cursor_up;
    engine_class->cursor_down = on_cursor_down;
}

int main(int argc, char** argv)
{
    ibus_init();

    IBusBus* bus = ibus_bus_new();
    if (ibus_bus_is_connected(bus) == FALSE) {
        g_printerr("NexKey: cannot connect to the IBus daemon\n");
        g_object_unref(bus);
        return 1;
    }
    g_object_ref_sink(bus);

    IBusFactory* factory = ibus_factory_new(ibus_bus_get_connection(bus));
    g_object_ref_sink(factory);
    ibus_factory_add_engine(factory, kEngineName, NEXKEY_TYPE_IBUS_ENGINE);

    const guint32 reply = ibus_bus_request_name(bus, kBusName, 0);
    if (reply == 0) {
        g_printerr("NexKey: cannot request the bus name %s\n", kBusName);
        g_object_unref(factory);
        g_object_unref(bus);
        return 1;
    }

    ibus_main();

    g_object_unref(factory);
    g_object_unref(bus);
    static_cast<void>(argc);
    static_cast<void>(argv);
    return 0;
}
