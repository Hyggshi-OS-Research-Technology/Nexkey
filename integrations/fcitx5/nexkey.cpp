// SPDX-License-Identifier: MIT
// NexKey - Fcitx5 input method addon.

#include "nexkey/Engine.hpp"
#include "nexkey/Settings.hpp"
#include "nexkey/Types.hpp"

#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addoninstance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>

#include <chrono>
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
    {
        reload_settings(true);
    }

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

        auto* ic = event.inputContext();
        if (ic == nullptr) {
            return;
        }

        const auto key = event.key();
        const auto states = key.states();

        // The word selection window comes first: its keys must not reach the
        // word parser.
        if (handle_candidate_key(ic, key)) {
            event.filterAndAccept();
            return;
        }

        if (states.test(fcitx::KeyState::Ctrl) || states.test(fcitx::KeyState::Alt) ||
            states.test(fcitx::KeyState::Super)) {
            commit_all(ic);
            return;
        }

        if (!settings_.enabled) {
            commit_all(ic);
            return;
        }

        if (key.sym() == FcitxKey_BackSpace) {
            const auto step = engine_.backspace();
            if (!step.handled) {
                commit_all(ic);
                return;
            }
            update_preedit(ic);
            event.filterAndAccept();
            return;
        }

        if (key.sym() == FcitxKey_Return || key.sym() == FcitxKey_KP_Enter ||
            key.sym() == FcitxKey_Escape || key.sym() == FcitxKey_Tab) {
            commit_all(ic);
            return;
        }

        const auto code_point = fcitx::Key::keySymToUnicode(key.sym());
        if (code_point == 0) {
            commit_all(ic);
            return;
        }

        const auto step = engine_.type(static_cast<char32_t>(code_point));
        if (!step.handled) {
            commit_all(ic);
            return;
        }
        if (!step.commit.empty()) {
            ic->commitString(step.commit);
        }
        update_preedit(ic);
        event.filterAndAccept();
    }

    void activate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override
    {
        static_cast<void>(entry);
        static_cast<void>(event);
        reload_settings(true);
        engine_.discard();
    }

    void deactivate(const fcitx::InputMethodEntry& entry,
                    fcitx::InputContextEvent& event) override
    {
        static_cast<void>(entry);
        if (auto* ic = event.inputContext(); ic != nullptr) {
            commit_all(ic);
        }
        engine_.discard();
    }

    void reset(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override
    {
        static_cast<void>(entry);
        if (auto* ic = event.inputContext(); ic != nullptr) {
            update_preedit(ic);
        }
        engine_.discard();
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
    void commit_all(fcitx::InputContext* ic)
    {
        const auto step = engine_.flush();
        if (!step.commit.empty()) {
            ic->commitString(step.commit);
        }
        update_preedit(ic);
    }

    void update_preedit(fcitx::InputContext* ic)
    {
        if (ic == nullptr) {
            return;
        }
        const std::string preedit(engine_.preedit());
        auto& panel = ic->inputPanel();
        if (preedit.empty()) {
            panel.setClientPreedit(fcitx::Text{});
        } else {
            panel.setClientPreedit(fcitx::Text(preedit));
        }
        update_candidates(ic);
        ic->updatePreedit();
    }

    /// Publish the alternative interpretations of the current word.
    void update_candidates(fcitx::InputContext* ic)
    {
        auto& panel = ic->inputPanel();

        if (!settings_.options.candidates) {
            panel.setCandidateList(nullptr);
            return;
        }

        const auto alternatives = engine_.candidates();
        if (alternatives.size() < 2) {
            panel.setCandidateList(nullptr);
            return;
        }

        auto list = std::make_unique<fcitx::CommonCandidateList>();
        list->setPageSize(static_cast<int>(alternatives.size()));
        list->setSelectionKey(selection_keys());

        for (const auto& alternative : alternatives) {
            auto word = std::make_unique<NexKeyCandidate>(
                alternative.text, alternative.comment, alternative.text,
                [this, ic, text = alternative.text] {
                    engine_.discard();
                    if (!text.empty()) {
                        ic->commitString(text);
                    }
                    update_preedit(ic);
                });
            list->append(std::move(word));
        }
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

    /// Returns true when the key selected a candidate.
    bool handle_candidate_key(fcitx::InputContext* ic, const fcitx::Key& key)
    {
        if (!engine_.composing()) {
            return false;
        }
        const auto list = ic->inputPanel().candidateList();
        if (!list) {
            return false;
        }

        const auto symbol = key.sym();
        if (symbol < FcitxKey_1 || symbol > FcitxKey_9) {
            return false;
        }

        // Digits are tone and modifier markers in VNI and literal text in VIQR,
        // so there they only select a candidate together with Control.
        const bool digits_are_text = settings_.options.method == nexkey::Method::Vni ||
                                     settings_.options.method == nexkey::Method::Viqr;
        const bool with_ctrl = key.states().test(fcitx::KeyState::Ctrl);
        if (digits_are_text && !with_ctrl) {
            return false;
        }

        const int index = static_cast<int>(symbol - FcitxKey_1);
        if (index >= list->size()) {
            return false;
        }

        engine_.discard();
        const auto text = static_cast<const NexKeyCandidate&>(list->candidate(index)).commit_text();
        if (!text.empty()) {
            ic->commitString(text);
        }
        update_preedit(ic);
        return true;
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
        engine_.discard();
        if (!nexkey::save_settings(settings_)) {
            FCITX_WARN() << "NexKey: cannot write the configuration file";
        }
        config_time_ = std::filesystem::file_time_type{};
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
        engine_.set_options(settings_.options);
        toggle_ = parse_toggle_key(settings_.toggle_key);
    }

    fcitx::Instance* instance_;
    nexkey::Engine engine_;
    nexkey::Settings settings_;
    ToggleKey toggle_;
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
