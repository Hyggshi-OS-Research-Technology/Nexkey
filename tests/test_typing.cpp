// SPDX-License-Identifier: MIT
// The whole direct typing path against a document, not just a word in isolation.
//
// Every case here types into a document that already has text in front of the
// word, because that is what a real application reports to the input method, and
// because a session that only ever sees its own word never notices that its
// mirror and the document disagree.

#include "TestHarness.hpp"

#include "ClientSim.hpp"

#include <string>

using sim::Behaviour;
using sim::Client;
using sim::InputMethod;

namespace {

/// A document with text in front of the word, like every real editor.
Client document(Behaviour behaviour = Behaviour::Honest)
{
    return Client(behaviour, "Xin ch\xC3\xA0o ");
}

} // namespace

// ---------------------------------------------------------------------------
// The reported symptoms: a word typed into a document that is not empty
// ---------------------------------------------------------------------------

TEST(typing_a_word_after_other_text_keeps_the_document_right)
{
    InputMethod method;
    auto client = document();

    // c, a, a, s -> "c\u1EA5"
    sim::type_all(method, client, "caas");
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c\xE1\xBA\xA5"));
}

TEST(typing_a_tone_on_a_word_that_ends_a_sentence_works)
{
    InputMethod method;
    auto client = document();
    sim::type_all(method, client, "tieengs");
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o ti\xE1\xBA\xBFng"));
}

TEST(removing_a_diacritic_with_backspace_keeps_the_letter)
{
    InputMethod method;
    auto client = document();
    sim::type_all(method, client, "caas");
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c\xE1\xBA\xA5"));

    // Backspace removes the tone and keeps the letter underneath: "c\u1EA5" ->
    // "c\xC3\xA2" -> "ca". The last character changes, so this is a replacement
    // and not a deletion of the end of the word.
    method.backspace(client);
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c\xC3\xA2"));
    method.backspace(client);
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o ca"));

    // ... and only then does a BackSpace reach the application and delete the
    // letter itself.
    method.backspace(client);
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c"));
}

TEST(a_word_boundary_commits_the_word_and_the_separator_together)
{
    InputMethod method;
    auto client = document();
    sim::type_all(method, client, "caas ");
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c\xE1\xBA\xA5 "));

    // The next word starts from an empty mirror, and it still lands correctly.
    sim::type_all(method, client, "hoaf");
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c\xE1\xBA\xA5 ho\xC3\xA0"));
}

TEST(a_client_that_ignores_deletions_still_ends_with_the_right_text)
{
    // The interesting case, and the one the bug report describes: a client that
    // accepts deleteSurroundingText and does nothing. The word must be repaired
    // rather than left as "c\xC3\xA2\xE1\xBA\xA5".
    InputMethod method;
    auto client = document(Behaviour::IgnoresDeletion);

    sim::type_all(method, client, "caas");
    EXPECT_TRUE(method.repairs() >= 0);
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c\xE1\xBA\xA5"));
}

TEST(a_client_that_ignores_deletions_keeps_working_for_the_next_word)
{
    // The repair only covers the word it was found in. The next word starts on
    // the normal path again and is repaired on its own, so a user who keeps
    // typing never sees a wrong letter on the screen.
    InputMethod method;
    auto client = document(Behaviour::IgnoresDeletion);

    sim::type_all(method, client, "caas ");
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c\xE1\xBA\xA5 "));

    sim::type_all(method, client, "hoaf");
    EXPECT_EQ(client.document(),
              std::string("Xin ch\xC3\xA0o c\xE1\xBA\xA5 ho\xC3\xA0"));
}

TEST(a_client_that_reports_its_text_late_is_not_repaired_twice)
{
    // A report that is one edit behind must not be mistaken for a deletion the
    // client ignored: repairing here would delete a character that is there.
    InputMethod method;
    auto client = document(Behaviour::StaleReport);

    sim::type_all(method, client, "caas");
    EXPECT_EQ(method.repairs(), 0);
    client.settle();
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c\xE1\xBA\xA5"));
}

TEST(a_client_without_surrounding_text_is_typed_into_the_same_way)
{
    InputMethod method;
    auto client = document(Behaviour::NoSurroundingText);
    sim::type_all(method, client, "caas");
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c\xE1\xBA\xA5"));
}

TEST(the_word_is_dropped_when_the_document_is_not_the_one_we_typed_into)
{
    InputMethod method;
    auto client = document();
    sim::type_all(method, client, "ca");

    // Somebody else typed there, or the cursor jumped: the half typed word is
    // dropped instead of being written on top of unknown text.
    auto moved = Client(Behaviour::Honest, "something else entirely");
    method.type('a', moved);
    EXPECT_TRUE(method.dropped());
}

// ---------------------------------------------------------------------------
// Backspace past the start of the word
// ---------------------------------------------------------------------------

TEST(backspace_at_the_start_of_a_word_reaches_the_application)
{
    InputMethod method;
    auto client = document();
    sim::type_all(method, client, "ca");
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o ca"));

    // "ca" -> "c": the word shrinks by one character, which the application
    // does itself.
    method.backspace(client);
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c"));

    // "c" -> nothing: still one character, still the application.
    method.backspace(client);
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o "));

    // The composition is over, so the next BackSpace deletes the space the way
    // the user expects, and the input method does not eat it.
    method.backspace(client);
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o"));
}

// ---------------------------------------------------------------------------
// The edits themselves
// ---------------------------------------------------------------------------

TEST(a_replacement_deletes_only_the_characters_it_replaces)
{
    InputMethod method;
    auto client = document();

    // "c" and "a" are appends and delete nothing.
    method.type('c', client);
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c"));
    method.type('a', client);
    EXPECT_EQ(method.last_deletes(), 0);
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o ca"));

    // "a" again turns the a into \xC3\xA2: exactly one character goes, and one
    // comes back. A count of two here is what used to eat the c.
    method.type('a', client);
    EXPECT_EQ(method.last_deletes(), 1);
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c\xC3\xA2"));

    // ... and the tone on top of it is one more.
    method.type('s', client);
    EXPECT_EQ(method.last_deletes(), 1);
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c\xE1\xBA\xA5"));
}

TEST(a_candidate_that_names_another_reading_replaces_the_word)
{
    InputMethod method;
    auto client = document();
    sim::type_all(method, client, "caas");
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c\xE1\xBA\xA5"));

    // Choosing "c\xC3\xA1" in the word selection window rewrites the tail and
    // keeps the rest of the document.
    method.commit_text("c\xC3\xA1", client);
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o c\xC3\xA1"));
}

TEST(space_after_a_candidate_keeps_both_the_word_and_the_space)
{
    InputMethod method;
    auto client = document();
    sim::type_all(method, client, "tieengs");
    method.commit_text("ti\xE1\xBA\xBFng ", client);
    EXPECT_EQ(client.document(), std::string("Xin ch\xC3\xA0o ti\xE1\xBA\xBFng "));
}

// ---------------------------------------------------------------------------
// Sequences from the bug report, in a document that starts empty as well
// ---------------------------------------------------------------------------

TEST(the_same_sequences_work_in_an_empty_document)
{
    struct Case {
        const char* keys;
        const char* expected;
    };
    const Case cases[] = {
        {"caas", "c\xE1\xBA\xA5"},              // cấ
        {"cas", "c\xC3\xA1"},                   // cá
        {"tieengs", "ti\xE1\xBA\xBFng"},        // tiếng
        {"hoaf", "ho\xC3\xA0"},                 // hoà
        {"ddi", "\xC4\x91i"},                   // đi
        {"vieetj", "vi\xE1\xBB\x87t"},          // việt
    };

    for (const auto& item : cases) {
        InputMethod method;
        Client client;
        sim::type_all(method, client, item.keys);
        EXPECT_EQ(client.document(), std::string(item.expected));
    }
}
