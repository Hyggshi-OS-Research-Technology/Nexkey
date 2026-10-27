// SPDX-License-Identifier: MIT
// NexKey - Fcitx5 input method addon.

#include "nexkey/Engine.hpp"
#include "nexkey/Settings.hpp"
#include "nexkey/Status.hpp"
#include "nexkey/Types.hpp"
#include "nexkey/Unicode.hpp"

#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>
#include <fcitx/userinterface.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;

constexpr std::string_view kViIcon{"nexkey-vn"};
constexpr std::string_view kEnIcon{"nexkey-en"};
constexpr std::string_view kPropertyName{"nexkey-state"};

struct ToggleKey {
    fcitx::Key key{fcitx::Key(FcitxKey_None)};
    fcitx::KeyStates modifiers{};

    [[nodiscard]] bool valid() const
    {
        return key.sym() != FcitxKey_None || modifiers != fcitx::KeyStates{};
    }

    /// True when the key name part of the configuration is empty, which makes
    /// this a modifier-only shortcut such as "Control+Shift".
    [[nodiscard]] bool modifier_only() const { return key.sym() == FcitxKey_None; }
};

fcitx::KeyStates modifier_states(std::string_view name)
{
    if (name == "control" || name == "ctrl") {
        return fcitx::KeyStates(fcitx::KeyState::Ctrl);
    }
    if (name == "shift") {
        return fcitx::KeyStates(fcitx::KeyState::Shift);
    }
    if (name == "alt" || name == "meta") {
        return fcitx::KeyStates(fcitx::KeyState::Alt);
    }
    if (name == "super" || name == "windows") {
        return fcitx::KeyStates(fcitx::KeyState::Super);
    }
    return fcitx::KeyStates{};
}

std::string lowercase(std::string_view text)
{
    std::string out(text);
    for (auto& character : out) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return out;
}

/// Parse a portable shortcut such as "Control+space" or "Control+Shift".
ToggleKey parse_toggle_key(std::string_view text)
{
    ToggleKey result;
    fcitx::KeySym symbol = FcitxKey_None;

    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find('+', start);
        const auto part = text.substr(start, end == std::string_view::npos ? text.size() - start
                                                                          : end - start);
        if (!part.empty()) {
            if (const auto states = modifier_states(lowercase(part)); states != fcitx::KeyStates{}) {
                result.modifiers = result.modifiers | states;
            } else {
                symbol = fcitx::Key::keySymFromString(std::string(part));
            }
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }

    result.key = fcitx::Key(symbol, result.modifiers);
    return result;
}

/// Everything that belongs to one input context.
///
/// Fcitx 5 hands every window, tab and text field its own input context and
/// switches between them without notice, so the composition must live here and
/// not in the addon: otherwise a half typed word of one window leaks into
/// another. The configuration on the other hand is shared by all of them.
class NexKeyState final : public fcitx::InputContextProperty {
public:
    nexkey::Engine engine;
    /// Value of the addon wide configuration generation the options above
    /// were taken from.
    unsigned generation{0};
    /// Characters already committed directly to the client in direct typing mode.
    std::string committed_text;
    /// Times at which the document did not contain what direct typing wrote.
    std::vector<std::chrono::steady_clock::time_point> mismatches;
};

/// The text helpers live in the core library so that the direct typing path and
/// the engine agree on what a character is. The local versions counted a byte
/// that is not a continuation byte as a character, which splits an overlong or
/// truncated sequence into pieces and lets a deletion land inside a letter.
inline std::size_t utf8_char_count(std::string_view text) noexcept
{
    return nexkey::unicode::count_code_points(text);
}

/// The first `chars` characters of a UTF-8 string, cut on a character boundary.
inline std::string utf8_prefix_chars(std::string_view text, std::size_t chars)
{
    return std::string(text.substr(0, nexkey::unicode::prefix_length(text, chars)));
}

void delete_characters(fcitx::InputContext* ic, std::size_t count)
{
    if (count == 0 || ic == nullptr) {
        return;
    }
    if (ic->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText) &&
        ic->surroundingText().isValid()) {
        const auto& st = ic->surroundingText();
        if (st.cursor() != st.anchor()) {
            const int sel_len = std::abs(static_cast<int>(st.cursor()) - static_cast<int>(st.anchor()));
            const int sel_start = std::min(static_cast<int>(st.cursor()), static_cast<int>(st.anchor())) -
                                  static_cast<int>(st.cursor());
            ic->deleteSurroundingText(sel_start, static_cast<unsigned int>(sel_len));
        }
        ic->deleteSurroundingText(-static_cast<int>(count), static_cast<unsigned int>(count));
    } else {
        for (std::size_t i = 0; i < count; ++i) {
            ic->forwardKey(fcitx::Key(FcitxKey_BackSpace), false);
            ic->forwardKey(fcitx::Key(FcitxKey_BackSpace), true);
        }
    }
}

/// A candidate of the word selection window. Choosing it commits the text it
/// carries and ends the composition.
class NexKeyCandidate final : public fcitx::CandidateWord {
public:
    NexKeyCandidate(const std::string& text, const std::string& comment, std::string commit,
                    std::function<void()> onSelect)
        : commit_(std::move(commit))
        , onSelect_(std::move(onSelect))
    {
        // Fcitx5 only grew a separate comment field in 5.1.9, so the note is
        // part of the label to work with older releases as well.
        const auto label = comment.empty() ? text : text + " - " + comment;
        setText(fcitx::Text(label));
    }

    void select(fcitx::InputContext* inputContext) const override
    {
        static_cast<void>(inputContext);
        onSelect_();
    }

    [[nodiscard]] const std::string& commit_text() const { return commit_; }

private:
    std::string commit_;
    std::function<void()> onSelect_;
};

class NexKeyEngine final : public fcitx::InputMethodEngine {
public:
    explicit NexKeyEngine(fcitx::Instance* instance)
        : instance_(instance)
        , state_factory_([this](fcitx::InputContext& inputContext) {
            static_cast<void>(inputContext);
            auto* state = new NexKeyState;
            state->generation = generation_;
            state->engine.set_options(settings_.options);
            state->engine.set_macros(settings_.macros);
            return state;
        })
    {
        // Never let one input context see the composition of another. This also
        // makes the state available to every input context that appears later.
        if (!instance_->inputContextManager().registerProperty(
                std::string(kPropertyName), &state_factory_)) {
            FCITX_WARN() << "NexKey: the input context property is already registered";
        }
        reload_settings(true);
    }

    NexKeyEngine(const NexKeyEngine&) = delete;
    NexKeyEngine& operator=(const NexKeyEngine&) = delete;

    ~NexKeyEngine() override { state_factory_.unregister(); }

    void keyEvent(const fcitx::InputMethodEntry& entry, fcitx::KeyEvent& event) override
    {
        static_cast<void>(entry);
        reload_settings();

        if (handle_toggle_key(event)) {
            event.filterAndAccept();
            return;
        }
        if (event.isRelease()) {
            return;
        }

        const auto key = event.key();
        if (key.isModifier()) {
            return;
        }

        auto* ic = event.inputContext();
        if (ic == nullptr) {
            return;
        }

        auto& state = state_of(ic);

        // The word selection window comes first: its keys must not reach the
        // word parser. It also decides on its own when to keep the key, so the
        // return value is all that matters here.
        if (handle_candidate_key(ic, state, key)) {
            event.filterAndAccept();
            return;
        }

        const auto states = key.states();
        if (states.test(fcitx::KeyState::Ctrl) || states.test(fcitx::KeyState::Alt) ||
            states.test(fcitx::KeyState::Super)) {
            commit_all(ic, state);
            return;
        }

        if (!settings_.enabled) {
            commit_all(ic, state);
            return;
        }

        // App Rules: check if application is configured to bypass Vietnamese input
        if (!settings_.app_rules.empty()) {
            if (auto it = settings_.app_rules.find(ic->program()); it != settings_.app_rules.end()) {
                if (it->second == "off" || it->second == "en" || it->second == "english") {
                    commit_all(ic, state);
                    return;
                }
            }
        }

        // Verify that earlier direct typing really reached the document, learn
        // from applications where it does not, and publish the status for the
        // diagnostics page.
        check_direct_health(ic, state);
        publish_status(ic, state);

        if (key.sym() == FcitxKey_BackSpace) {
            const auto step = state.engine.backspace();
            if (!step.handled) {
                commit_all(ic, state);
                return;
            }
            if (use_direct_typing(ic)) {
                // Pure deletion of the last character (e.g. "kh" -> "k"): let the
                // application handle its own BackSpace instead of relying on
                // deleteSurroundingText, which many clients silently ignore.
                const std::string new_preedit(state.engine.preedit());
                const std::string& old_text = state.committed_text;
                if (new_preedit.size() < old_text.size() &&
                    old_text.compare(0, new_preedit.size(), new_preedit) == 0 &&
                    utf8_char_count(old_text) - utf8_char_count(new_preedit) == 1) {
                    state.committed_text = new_preedit;
                    ic->inputPanel().setClientPreedit(fcitx::Text{});
                    update_candidates(ic, state);
                    ic->updatePreedit();
                    return; // key not filtered: the app deletes the character
                }
            }
            update_preedit(ic, state);
            event.filterAndAccept();
            return;
        }

        if (key.sym() == FcitxKey_Return || key.sym() == FcitxKey_KP_Enter ||
            key.sym() == FcitxKey_Escape || key.sym() == FcitxKey_Tab ||
            key.sym() == FcitxKey_Left || key.sym() == FcitxKey_Right ||
            key.sym() == FcitxKey_Up || key.sym() == FcitxKey_Down ||
            key.sym() == FcitxKey_Home || key.sym() == FcitxKey_End) {
            commit_all(ic, state);
            return;
        }

        const auto code_point = fcitx::Key::keySymToUnicode(key.sym());
        if (code_point == 0) {
            commit_all(ic, state);
            return;
        }

        const auto step = state.engine.type(static_cast<char32_t>(code_point));
        if (!step.handled) {
            commit_all(ic, state);
            return;
        }
        if (!step.commit.empty()) {
            if (use_direct_typing(ic)) {
                const auto prefix = nexkey::unicode::common_prefix(state.committed_text, step.commit);
                const std::size_t old_total_chars = utf8_char_count(state.committed_text);
                const std::size_t chars_to_delete = old_total_chars - prefix.code_points;
                if (chars_to_delete > 0) {
                    delete_characters(ic, chars_to_delete);
                }
                if (prefix.bytes < step.commit.size()) {
                    const std::string suffix = step.commit.substr(prefix.bytes);
                    ic->commitString(suffix);
                }
                state.committed_text.clear();
            } else {
                ic->commitString(step.commit);
            }
        }
        update_preedit(ic, state);
        event.filterAndAccept();
    }

    void activate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override
    {
        static_cast<void>(entry);
        reload_settings(true);
        if (auto* ic = event.inputContext(); ic != nullptr) {
            auto& state = state_of(ic);
            commit_all(ic, state);
            publish_status(ic, state);
        }
    }

    void deactivate(const fcitx::InputMethodEntry& entry,
                    fcitx::InputContextEvent& event) override
    {
        static_cast<void>(entry);
        if (auto* ic = event.inputContext(); ic != nullptr) {
            commit_all(ic, state_of(ic));
        }
    }

    void reset(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override
    {
        static_cast<void>(entry);
        if (auto* ic = event.inputContext(); ic != nullptr) {
            auto& state = state_of(ic);
            commit_all(ic, state);
        }
    }

    std::string subMode(const fcitx::InputMethodEntry& entry,
                        fcitx::InputContext& inputContext) override
    {
        static_cast<void>(entry);
        static_cast<void>(inputContext);
        reload_settings();
        return settings_.enabled ? "VI" : "EN";
    }

    std::string overrideIcon(const fcitx::InputMethodEntry& entry) override
    {
        static_cast<void>(entry);
        reload_settings();
        return std::string(settings_.enabled ? kViIcon : kEnIcon);
    }

private:
    /// Per input context state, with the options brought up to date when the
    /// configuration changed since the last use.
    NexKeyState& state_of(fcitx::InputContext* ic)
    {
        auto* state = ic->propertyFor(&state_factory_);
        if (state->generation != generation_) {
            state->generation = generation_;
            auto opts = settings_.options;
            if (ic != nullptr && !settings_.app_rules.empty()) {
                const auto& prog = ic->program();
                if (auto it = settings_.app_rules.find(prog); it != settings_.app_rules.end()) {
                    if (auto m = nexkey::method_from_string(it->second)) {
                        opts.method = *m;
                    }
                }
            }
            state->engine.set_options(opts);
            state->engine.set_macros(settings_.macros);
            state->engine.set_custom_words(settings_.custom_dictionary);
        }
        return *state;
    }

    struct DirectDecision {
        bool direct{true};
        const char* reason{""};
    };

    /// Decide between direct typing and preedit for one input context.
    DirectDecision direct_decision(fcitx::InputContext* ic) const
    {
        if (!settings_.options.direct_typing) {
            return {false, "Đã tắt gõ trực tiếp trong cài đặt"};
        }
        if (ic == nullptr) {
            return {true, "Mặc định"};
        }
        const std::string& program = ic->program();
        if (auto it = settings_.app_rules.find(program); it != settings_.app_rules.end()) {
            if (it->second == "preedit") {
                if (!last_learned_.empty() && program == last_learned_) {
                    return {false, "Tự học: xóa/thay chữ không hiệu lực ở ứng dụng này"};
                }
                return {false, "Quy tắc ứng dụng: preedit"};
            }
            if (it->second == "direct") {
                return {true, "Quy tắc ứng dụng: direct"};
            }
        }
        if (program == "gnome-text-editor") {
            return {false, "Danh sách có sẵn (gnome-text-editor)"};
        }
        // Chromium / Electron based programs ignore deleteSurroundingText, so
        // direct typing cannot replace or delete characters there.
        // Program names are ASCII and the match must not follow the locale.
        std::string lower = program;
        for (char& c : lower) {
            if (c >= 'A' && c <= 'Z') {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        for (const char* needle : {"chrome", "chromium", "brave", "firefox", "electron",
                                   "code", "discord", "slack", "obsidian"}) {
            if (lower.find(needle) != std::string::npos) {
                return {false, "Danh sách có sẵn (Chromium/Electron)"};
            }
        }
        return {true, "Mặc định"};
    }

    bool use_direct_typing(fcitx::InputContext* ic) const
    {
        return direct_decision(ic).direct;
    }

    /// Compare the document with what direct typing believes it wrote. Three
    /// mismatches within a minute mean the application ignores deletions, so
    /// it is switched to preedit for good.
    void check_direct_health(fcitx::InputContext* ic, NexKeyState& state)
    {
        const auto now = std::chrono::steady_clock::now();
        std::erase_if(state.mismatches, [now](const auto& when) { return now - when > 60s; });

        if (state.committed_text.empty() || !use_direct_typing(ic)) {
            return;
        }
        if (!ic->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText)) {
            return;
        }
        const auto& surrounding = ic->surroundingText();
        if (!surrounding.isValid() || surrounding.cursor() != surrounding.anchor()) {
            return;
        }

        const std::string before = utf8_prefix_chars(surrounding.text(), surrounding.cursor());
        const std::string& expected = state.committed_text;
        if (before.size() >= expected.size() &&
            before.compare(before.size() - expected.size(), expected.size(), expected) == 0) {
            return;
        }

        // The document does not hold what was typed: the cursor may simply have
        // moved, so only drop the composition and count the event.
        state.mismatches.push_back(now);
        commit_all(ic, state);
        if (state.mismatches.size() >= 3) {
            learn_preedit(ic, state);
        }
    }

    /// Remember that direct typing does not work in this application.
    void learn_preedit(fcitx::InputContext* ic, NexKeyState& state)
    {
        const std::string program = ic->program();
        if (program.empty()) {
            return;
        }
        // Still in direct mode here, so this drops the state without committing.
        commit_all(ic, state);

        settings_.app_rules[program] = "preedit";
        ++generation_;
        last_learned_ = program;
        last_learned_at_ = static_cast<std::int64_t>(std::time(nullptr));
        state.mismatches.clear();
        if (!nexkey::save_settings(settings_)) {
            FCITX_WARN() << "NexKey: could not save the learned rule for " << program;
        }
        FCITX_INFO() << "NexKey: switched " << program << " to preedit mode";
        publish_status(ic, state);
    }

    /// Write the diagnostics snapshot, at most when something changed or every
    /// few seconds.
    void publish_status(fcitx::InputContext* ic, NexKeyState& state)
    {
        if (ic == nullptr) {
            return;
        }
        const std::string program = ic->program();
        if (program.empty() || program == "nexkey-gui") {
            return;
        }

        const auto decision = direct_decision(ic);
        const bool capability =
            ic->capabilityFlags().test(fcitx::CapabilityFlag::SurroundingText);
        const bool valid = capability && ic->surroundingText().isValid();
        const int recent = static_cast<int>(state.mismatches.size());

        const std::string key = program + '|' + (decision.direct ? 'd' : 'p') + '|' +
                                decision.reason + '|' + (capability ? '1' : '0') +
                                (valid ? '1' : '0') + std::to_string(recent) + last_learned_;
        const auto now = std::chrono::steady_clock::now();
        if (key == last_status_key_ && now - last_status_write_ < 5s) {
            return;
        }
        last_status_key_ = key;
        last_status_write_ = now;

        nexkey::RuntimeStatus status;
        status.program = program;
        status.mode = decision.direct ? "direct" : "preedit";
        status.reason = decision.reason;
        status.surrounding_capability = capability;
        status.surrounding_valid = valid;
        status.recent_mismatches = recent;
        status.addon_version = std::string(nexkey::kVersion);
        status.addon_build = std::string(nexkey::build_id());
        status.unicode_backend = std::string(nexkey::unicode::backend_name());
        status.updated = static_cast<std::int64_t>(std::time(nullptr));
        status.last_learned = last_learned_;
        status.last_learned_at = last_learned_at_;
        static_cast<void>(nexkey::write_status(status));
    }

    void commit_all(fcitx::InputContext* ic, NexKeyState& state)
    {
        if (use_direct_typing(ic)) {
            state.engine.discard();
            state.committed_text.clear();
            auto& panel = ic->inputPanel();
            panel.setClientPreedit(fcitx::Text{});
            panel.setCandidateList(nullptr);
            ic->updatePreedit();
        } else {
            const auto step = state.engine.flush();
            if (!step.commit.empty()) {
                ic->commitString(step.commit);
            }
            update_preedit(ic, state);
        }
    }

    /// Commit the text of one candidate and end the composition.
    void commit_candidate(fcitx::InputContext* ic, NexKeyState& state, const std::string& text)
    {
        if (use_direct_typing(ic)) {
            const auto prefix = nexkey::unicode::common_prefix(state.committed_text, text);
            const std::size_t old_total_chars = utf8_char_count(state.committed_text);
            const std::size_t chars_to_delete = old_total_chars - prefix.code_points;
            if (chars_to_delete > 0) {
                delete_characters(ic, chars_to_delete);
            }
            state.engine.discard();
            state.committed_text.clear();
            if (prefix.bytes < text.size()) {
                const std::string suffix = text.substr(prefix.bytes);
                ic->commitString(suffix);
            }
            update_preedit(ic, state);
        } else {
            state.engine.discard();
            if (!text.empty()) {
                ic->commitString(text);
            }
            update_preedit(ic, state);
        }
    }

    void update_preedit(fcitx::InputContext* ic, NexKeyState& state)
    {
        if (ic == nullptr) {
            return;
        }
        if (use_direct_typing(ic)) {
            const std::string new_preedit(state.engine.preedit());
            if (new_preedit != state.committed_text) {
                const auto prefix = nexkey::unicode::common_prefix(state.committed_text, new_preedit);
                const std::size_t old_total_chars = utf8_char_count(state.committed_text);
                const std::size_t chars_to_delete = old_total_chars - prefix.code_points;
                if (chars_to_delete > 0) {
                    delete_characters(ic, chars_to_delete);
                }
                if (prefix.bytes < new_preedit.size()) {
                    const std::string suffix = new_preedit.substr(prefix.bytes);
                    ic->commitString(suffix);
                }
                state.committed_text = new_preedit;
            }
            auto& panel = ic->inputPanel();
            panel.setClientPreedit(fcitx::Text{});
            update_candidates(ic, state);
            ic->updatePreedit();
        } else {
            const std::string preedit(state.engine.preedit());
            auto& panel = ic->inputPanel();
            if (preedit.empty()) {
                panel.setClientPreedit(fcitx::Text{});
            } else {
                panel.setClientPreedit(fcitx::Text(preedit));
            }
            update_candidates(ic, state);
            ic->updatePreedit();
        }
    }

    /// Publish the alternative interpretations of the current word.
    void update_candidates(fcitx::InputContext* ic, NexKeyState& state)
    {
        auto& panel = ic->inputPanel();

        if (!settings_.options.candidates || !state.engine.composing()) {
            panel.setCandidateList(nullptr);
            return;
        }

        const auto alternatives = state.engine.candidates();
        if (alternatives.size() < 2) {
            panel.setCandidateList(nullptr);
            return;
        }

        auto list = std::make_unique<fcitx::CommonCandidateList>();
        list->setPageSize(static_cast<int>(alternatives.size()));
        list->setSelectionKey(selection_keys());

        for (const auto& alternative : alternatives) {
            list->append(std::make_unique<NexKeyCandidate>(
                alternative.text, alternative.comment, alternative.text,
                [this, ic, text = alternative.text] {
                    commit_candidate(ic, state_of(ic), text);
                    ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
                }));
        }
        // Start on the default entry so that space and the selection keys agree
        // on what is highlighted.
        list->setGlobalCursorIndex(0);
        panel.setCandidateList(std::move(list));
    }

    /// Keys that pick an entry of the word selection window.
    static fcitx::KeyList selection_keys()
    {
        fcitx::KeyList keys;
        for (int digit = 0; digit < 9; ++digit) {
            keys.emplace_back(static_cast<fcitx::KeySym>(FcitxKey_1 + digit));
        }
        return keys;
    }

    /// Index of a plain or Control prefixed digit, -1 for anything else.
    static int selection_index(const fcitx::Key& key, bool control_required)
    {
        const auto symbol = key.sym();
        if (symbol < FcitxKey_1 || symbol > FcitxKey_9) {
            return -1;
        }
        const bool with_ctrl = key.states().test(fcitx::KeyState::Ctrl);
        if (with_ctrl != control_required) {
            return -1;
        }
        return static_cast<int>(symbol - FcitxKey_1);
    }

    /// Returns true when the key belonged to the word selection window.
    bool handle_candidate_key(fcitx::InputContext* ic, NexKeyState& state, const fcitx::Key& key)
    {
        if (!state.engine.composing()) {
            return false;
        }
        const auto list = ic->inputPanel().candidateList();
        if (!list) {
            return false;
        }

        // Digits are tone and modifier markers in VNI and VIQR, so there they
        // only select a candidate together with Control.
        const bool digits_are_text = settings_.options.method == nexkey::Method::Vni ||
                                     settings_.options.method == nexkey::Method::Viqr;
        if (const int index = selection_index(key, digits_are_text); index >= 0) {
            if (index >= list->size()) {
                return false;
            }
            commit_candidate(ic, state,
                             static_cast<const NexKeyCandidate&>(list->candidate(index))
                                 .commit_text());
            return true;
        }

        const auto symbol = key.sym();

        if (auto* movable = list->toCursorMovable(); movable != nullptr) {
            if (symbol == FcitxKey_Up) {
                movable->prevCandidate();
                ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
                return true;
            }
            if (symbol == FcitxKey_Down) {
                movable->nextCandidate();
                ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
                return true;
            }
        }

        if (auto* pageable = list->toPageable(); pageable != nullptr) {
            if (symbol == FcitxKey_Page_Up && pageable->hasPrev()) {
                pageable->prev();
                ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
                return true;
            }
            if (symbol == FcitxKey_Page_Down && pageable->hasNext()) {
                pageable->next();
                ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
                return true;
            }
        }

        // Space commits the highlighted candidate (or preedit) followed by a space
        // in one atomic commit, avoiding double-commit issues in editors.
        if (symbol == FcitxKey_space) {
            const int index = list->cursorIndex();
            std::string text;
            if (index >= 0 && index < list->size()) {
                text = static_cast<const NexKeyCandidate&>(list->candidate(index)).commit_text();
            } else {
                text = state.engine.preedit();
            }
            state.engine.discard();
            if (!text.empty()) {
                text.push_back(' ');
                ic->commitString(text);
            }
            update_preedit(ic, state);
            return true;
        }
        return false;
    }

    bool handle_toggle_key(const fcitx::KeyEvent& event)
    {
        const auto key = event.key();
        if (!toggle_.valid()) {
            return false;
        }

        if (toggle_.modifier_only()) {
            if (!event.isRelease() || !key.isModifier()) {
                return false;
            }
            if (!key.states().test(toggle_.modifiers)) {
                return false;
            }
            toggle_mode();
            return true;
        }

        if (event.isRelease()) {
            return false;
        }
        if (key.check(toggle_.key)) {
            toggle_mode();
            return true;
        }
        return false;
    }

    void toggle_mode()
    {
        settings_.enabled = !settings_.enabled;
        ++generation_;
        if (!nexkey::save_settings(settings_)) {
            FCITX_WARN() << "NexKey: cannot write the configuration file";
        }
        config_time_ = std::filesystem::file_time_type{};
        // Every input context starts over: a composition belongs to the mode
        // that produced it.
        instance_->inputContextManager().foreach([this](fcitx::InputContext* ic) {
            if (auto* property = ic->property(std::string(kPropertyName));
                property != nullptr) {
                auto& state = *static_cast<NexKeyState*>(property);
                commit_all(ic, state);
                if (state.generation != generation_) {
                    state.generation = generation_;
                    state.engine.set_options(settings_.options);
                    state.engine.set_macros(settings_.macros);
                }
                update_preedit(ic, state);
            }
            return true;
        });
        FCITX_INFO() << "NexKey: switched to " << (settings_.enabled ? "VI" : "EN") << " mode";
    }

    void reload_settings(bool force = false)
    {
        const auto now = std::chrono::steady_clock::now();
        if (!force && now - last_check_ < 250ms) {
            return;
        }
        last_check_ = now;

        const auto path = nexkey::default_config_path();
        std::error_code error;
        const auto modified = std::filesystem::last_write_time(path, error);
        if (!force && !error && modified == config_time_) {
            return;
        }
        if (!error) {
            config_time_ = modified;
        }

        settings_ = nexkey::load_settings(path);
        // A reloaded configuration has to reach the input contexts that are
        // already open, so the generation counter is bumped here as well.
        ++generation_;
        toggle_ = parse_toggle_key(settings_.toggle_key);
    }

    fcitx::Instance* instance_;
    nexkey::Settings settings_;
    ToggleKey toggle_;
    fcitx::FactoryFor<NexKeyState> state_factory_;
    unsigned generation_{0};
    std::string last_learned_;
    std::int64_t last_learned_at_{0};
    std::string last_status_key_;
    std::chrono::steady_clock::time_point last_status_write_{};
    std::filesystem::file_time_type config_time_{};
    std::chrono::steady_clock::time_point last_check_{};
};

class NexKeyAddonFactory final : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override
    {
        return new NexKeyEngine(manager->instance());
    }
};

} // namespace

FCITX_ADDON_FACTORY(NexKeyAddonFactory);
