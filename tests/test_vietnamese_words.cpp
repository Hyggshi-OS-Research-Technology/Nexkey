// SPDX-License-Identifier: MIT
// A Vietnamese word corpus typed through the engine.
//
// Every entry is one word as it appears in a document, together with the
// keystrokes that produce it in Telex and in VNI. The corpus covers the vowel
// clusters that the tone placement rules treat differently (oa, oe, uy, uye,
// ie, uo, uo+horn), all six tones, the stroked d, and the letters with two
// marks such as the a with a breve and a dot below: those are the ones a naive
// NFD encoder writes in the wrong order.
//
// The tests below do not only check the text each word ends up as: they also
// check that the engine's state after every keystroke is exactly what rendering
// the prefix from scratch would produce, that Backspace walks those states in
// reverse, and that nothing the engine hands to a frontend is malformed UTF-8.

#include "Helpers.hpp"
#include "TestHarness.hpp"

#include "nexkey/Converter.hpp"
#include "nexkey/Engine.hpp"
#include "nexkey/Unicode.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct WordCase {
    const char* telex; ///< Telex keystrokes, ASCII.
    const char* vni;   ///< VNI keystrokes, ASCII.
    const char* text;  ///< The word as it has to appear in the document.
};

constexpr WordCase kWords[]{
    {"ta", "ta", "ta"},
    {"ba", "ba", "ba"},
    {"cas", "ca1", "c\u00E1"},
    {"nhaf", "nha2", "nh\u00E0"},
    {"laf", "la2", "l\u00E0"},
    {"maf", "ma2", "m\u00E0"},
    {"car", "ca3", "c\u1EA3"},
    {"mai", "mai", "mai"},
    {"hai", "hai", "hai"},
    {"casi", "ca1i", "c\u00E1i"},
    {"vafi", "va2i", "v\u00E0i"},
    {"dafi", "da2i", "d\u00E0i"},
    {"phari", "pha3i", "ph\u1EA3i"},
    {"tasi", "ta1i", "t\u00E1i"},
    {"sao", "sao", "sao"},
    {"cao", "cao", "cao"},
    {"baso", "ba1o", "b\u00E1o"},
    {"ddaro", "d9a3o", "\u0111\u1EA3o"},
    {"chafo", "cha2o", "ch\u00E0o"},
    {"nafo", "na2o", "n\u00E0o"},
    {"sau", "sau", "sau"},
    {"ddau", "d9au", "\u0111au"},
    {"tafu", "ta2u", "t\u00E0u"},
    {"mafu", "ma2u", "m\u00E0u"},
    {"giafu", "gia2u", "gi\u00E0u"},
    {"tay", "tay", "tay"},
    {"hay", "hay", "hay"},
    {"nafy", "na2y", "n\u00E0y"},
    {"chajy", "cha5y", "ch\u1EA1y"},
    {"ddaafy", "d9a62y", "\u0111\u1EA7y"},
    {"thay", "thay", "thay"},
    {"ddaau", "d9a6u", "\u0111\u00E2u"},
    {"xaay", "xa6y", "x\u00E2y"},
    {"laau", "la6u", "l\u00E2u"},
    {"saau", "sa6u", "s\u00E2u"},
    {"caafu", "ca62u", "c\u1EA7u"},
    {"naasu", "na61u", "n\u1EA5u"},
    {"thaafy", "tha62y", "th\u1EA7y"},
    {"laasy", "la61y", "l\u1EA5y"},
    {"caay", "ca6y", "c\u00E2y"},
    {"awn", "a8n", "\u0103n"},
    {"lawn", "la8n", "l\u0103n"},
    {"bawn", "ba8n", "b\u0103n"},
    {"tawsn", "ta81n", "t\u1EAFn"},
    {"nawsng", "na81ng", "n\u1EAFng"},
    {"bawfng", "ba82ng", "b\u1EB1ng"},
    {"me", "me", "me"},
    {"xe", "xe", "xe"},
    {"tre", "tre", "tre"},
    {"quen", "quen", "quen"},
    {"keo", "keo", "keo"},
    {"heo", "heo", "heo"},
    {"meso", "me1o", "m\u00E9o"},
    {"nghefo", "nghe2o", "ngh\u00E8o"},
    {"kheso", "khe1o", "kh\u00E9o"},
    {"veef", "ve62", "v\u1EC1"},
    {"neen", "ne6n", "n\u00EAn"},
    {"teen", "te6n", "t\u00EAn"},
    {"been", "be6n", "b\u00EAn"},
    {"leen", "le6n", "l\u00EAn"},
    {"treen", "tre6n", "tr\u00EAn"},
    {"ddeef", "d9e62", "\u0111\u1EC1"},
    {"teest", "te61t", "t\u1EBFt"},
    {"heest", "he61t", "h\u1EBFt"},
    {"keest", "ke61t", "k\u1EBFt"},
    {"cheest", "che61t", "ch\u1EBFt"},
    {"ghees", "ghe61", "gh\u1EBF"},
    {"leej", "le65", "l\u1EC7"},
    {"theer", "the63", "th\u1EC3"},
    {"neeu", "ne6u", "n\u00EAu"},
    {"keeu", "ke6u", "k\u00EAu"},
    {"leefu", "le62u", "l\u1EC1u"},
    {"ddeefu", "d9e62u", "\u0111\u1EC1u"},
    {"ddi", "d9i", "\u0111i"},
    {"khi", "khi", "khi"},
    {"ghi", "ghi", "ghi"},
    {"gif", "gi2", "g\u00EC"},
    {"thif", "thi2", "th\u00EC"},
    {"chir", "chi3", "ch\u1EC9"},
    {"mif", "mi2", "m\u00EC"},
    {"tis", "ti1", "t\u00ED"},
    {"vis", "vi1", "v\u00ED"},
    {"bia", "bia", "bia"},
    {"misa", "mi1a", "m\u00EDa"},
    {"ddixa", "d9i4a", "\u0111\u0129a"},
    {"thifa", "thi2a", "th\u00ECa"},
    {"chia", "chia", "chia"},
    {"tieen", "tie6n", "ti\u00EAn"},
    {"mieefn", "mie62n", "mi\u1EC1n"},
    {"vieejc", "vie65c", "vi\u1EC7c"},
    {"tieesng", "tie61ng", "ti\u1EBFng"},
    {"ddieerm", "d9ie63m", "\u0111i\u1EC3m"},
    {"thieen", "thie6n", "thi\u00EAn"},
    {"dieexn", "die64n", "di\u1EC5n"},
    {"kieen", "kie6n", "ki\u00EAn"},
    {"lieeng", "lie6ng", "li\u00EAng"},
    {"yeeu", "ye6u", "y\u00EAu"},
    {"tieeu", "tie6u", "ti\u00EAu"},
    {"nhieefu", "nhie62u", "nhi\u1EC1u"},
    {"thieesu", "thie61u", "thi\u1EBFu"},
    {"hieeru", "hie63u", "hi\u1EC3u"},
    {"difu", "di2u", "d\u00ECu"},
    {"xisu", "xi1u", "x\u00EDu"},
    {"ddiju", "d9i5u", "\u0111\u1ECBu"},
    {"nisu", "ni1u", "n\u00EDu"},
    {"bo", "bo", "bo"},
    {"cho", "cho", "cho"},
    {"to", "to", "to"},
    {"no", "no", "no"},
    {"lo", "lo", "lo"},
    {"ddos", "d9o1", "\u0111\u00F3"},
    {"hoa", "hoa", "hoa"},
    {"toa", "toa", "toa"},
    {"khoa", "khoa", "khoa"},
    {"loa", "loa", "loa"},
    {"hoaf", "hoa2", "ho\u00E0"},
    {"khoas", "khoa1", "kho\u00E1"},
    {"toaf", "toa2", "to\u00E0"},
    {"khoai", "khoai", "khoai"},
    {"toaji", "toa5i", "to\u1EA1i"},
    {"thoaji", "thoa5i", "tho\u1EA1i"},
    {"ngoaji", "ngoa5i", "ngo\u1EA1i"},
    {"ngoaso", "ngoa1o", "ngo\u00E1o"},
    {"ngoao", "ngoao", "ngoao"},
    {"loay", "loay", "loay"},
    {"xoay", "xoay", "xoay"},
    {"ngoasy", "ngoa1y", "ngo\u00E1y"},
    {"hoawjc", "hoa85c", "ho\u1EB7c"},
    {"ngoawfn", "ngoa82n", "ngo\u1EB1n"},
    {"xoawn", "xoa8n", "xo\u0103n"},
    {"khoawn", "khoa8n", "kho\u0103n"},
    {"khoe", "khoe", "khoe"},
    {"loe", "loe", "loe"},
    {"hoe", "hoe", "hoe"},
    {"khoes", "khoe1", "kho\u00E9"},
    {"khoer", "khoe3", "kho\u1EBB"},
    {"ngoefo", "ngoe2o", "ngo\u00E8o"},
    {"ngoero", "ngoe3o", "ngo\u1EBBo"},
    {"khoefo", "khoe2o", "kho\u00E8o"},
    {"voi", "voi", "voi"},
    {"coi", "coi", "coi"},
    {"noi", "noi", "noi"},
    {"hosi", "ho1i", "h\u00F3i"},
    {"xosi", "xo1i", "x\u00F3i"},
    {"goji", "go5i", "g\u1ECDi"},
    {"cofi", "co2i", "c\u00F2i"},
    {"bosi", "bo1i", "b\u00F3i"},
    {"thosi", "tho1i", "th\u00F3i"},
    {"hori", "ho3i", "h\u1ECFi"},
    {"boo", "bo6", "b\u00F4"},
    {"coo", "co6", "c\u00F4"},
    {"oong", "o6ng", "\u00F4ng"},
    {"khoong", "kho6ng", "kh\u00F4ng"},
    {"soos", "so61", "s\u1ED1"},
    {"toos", "to61", "t\u1ED1"},
    {"hoo", "ho6", "h\u00F4"},
    {"mooj", "mo65", "m\u1ED9"},
    {"noofi", "no62i", "n\u1ED3i"},
    {"goosi", "go61i", "g\u1ED1i"},
    {"toosi", "to61i", "t\u1ED1i"},
    {"ddooi", "d9o6i", "\u0111\u00F4i"},
    {"xooi", "xo6i", "x\u00F4i"},
    {"khooi", "kho6i", "kh\u00F4i"},
    {"ngooi", "ngo6i", "ng\u00F4i"},
    {"bow", "bo7", "b\u01A1"},
    {"mow", "mo7", "m\u01A1"},
    {"thow", "tho7", "th\u01A1"},
    {"towf", "to72", "t\u1EDD"},
    {"cowf", "co72", "c\u1EDD"},
    {"nowj", "no75", "n\u1EE3"},
    {"chowj", "cho75", "ch\u1EE3"},
    {"vowx", "vo74", "v\u1EE1"},
    {"mowr", "mo73", "m\u1EDF"},
    {"ddowx", "d9o74", "\u0111\u1EE1"},
    {"bowi", "bo7i", "b\u01A1i"},
    {"chowi", "cho7i", "ch\u01A1i"},
    {"guwri", "gu73i", "g\u1EEDi"},
    {"cowri", "co73i", "c\u1EDFi"},
    {"ddowfi", "d9o72i", "\u0111\u1EDDi"},
    {"rowi", "ro7i", "r\u01A1i"},
    {"khowi", "kho7i", "kh\u01A1i"},
    {"xowi", "xo7i", "x\u01A1i"},
    {"bu", "bu", "bu"},
    {"tu", "tu", "tu"},
    {"thu", "thu", "thu"},
    {"chus", "chu1", "ch\u00FA"},
    {"muf", "mu2", "m\u00F9"},
    {"ngur", "ngu3", "ng\u1EE7"},
    {"ddur", "d9u3", "\u0111\u1EE7"},
    {"tuj", "tu5", "t\u1EE5"},
    {"thus", "thu1", "th\u00FA"},
    {"mua", "mua", "mua"},
    {"cura", "cu3a", "c\u1EE7a"},
    {"chua", "chua", "chua"},
    {"tua", "tua", "tua"},
    {"ddufa", "d9u2a", "\u0111\u00F9a"},
    {"musa", "mu1a", "m\u00FAa"},
    {"chufa", "chu2a", "ch\u00F9a"},
    {"cua", "cua", "cua"},
    {"vua", "vua", "vua"},
    {"tuaafn", "tua62n", "tu\u1EA7n"},
    {"xuaan", "xua6n", "xu\u00E2n"},
    {"huaan", "hua6n", "hu\u00E2n"},
    {"quaan", "qua6n", "qu\u00E2n"},
    {"khuaan", "khua6n", "khu\u00E2n"},
    {"chuaarn", "chua63n", "chu\u1EA9n"},
    {"tuaasn", "tua61n", "tu\u1EA5n"},
    {"nhuaajn", "nhua65n", "nhu\u1EADn"},
    {"khuaay", "khua6y", "khu\u00E2y"},
    {"quaay", "qua6y", "qu\u00E2y"},
    {"khuaasy", "khua61y", "khu\u1EA5y"},
    {"quaasy", "qua61y", "qu\u1EA5y"},
    {"quaafy", "qua62y", "qu\u1EA7y"},
    {"huee", "hue6", "hu\u00EA"},
    {"hueej", "hue65", "hu\u1EC7"},
    {"quees", "que61", "qu\u1EBF"},
    {"muoosi", "muo61i", "mu\u1ED1i"},
    {"tuoori", "tuo63i", "tu\u1ED5i"},
    {"chuoosi", "chuo61i", "chu\u1ED1i"},
    {"cuoojc", "cuo65c", "cu\u1ED9c"},
    {"buoojc", "buo65c", "bu\u1ED9c"},
    {"thuoosc", "thuo61c", "thu\u1ED1c"},
    {"uoosng", "uo61ng", "u\u1ED1ng"},
    {"muoon", "muo6n", "mu\u00F4n"},
    {"chuoong", "chuo6ng", "chu\u00F4ng"},
    {"nguoofn", "nguo62n", "ngu\u1ED3n"},
    {"cuoosn", "cuo61n", "cu\u1ED1n"},
    {"muoosn", "muo61n", "mu\u1ED1n"},
    {"nuooi", "nuo6i", "nu\u00F4i"},
    {"dduooi", "d9uo6i", "\u0111u\u00F4i"},
    {"cuoosi", "cuo61i", "cu\u1ED1i"},
    {"nguooi", "nguo6i", "ngu\u00F4i"},
    {"vui", "vui", "vui"},
    {"nusi", "nu1i", "n\u00FAi"},
    {"tusi", "tu1i", "t\u00FAi"},
    {"musi", "mu1i", "m\u00FAi"},
    {"chui", "chui", "chui"},
    {"buji", "bu5i", "b\u1EE5i"},
    {"dufi", "du2i", "d\u00F9i"},
    {"ddui", "d9ui", "\u0111ui"},
    {"cusi", "cu1i", "c\u00FAi"},
    {"thuyr", "thuy3", "thu\u1EF7"},
    {"quyf", "quy2", "qu\u1EF3"},
    {"tuyf", "tuy2", "tu\u1EF3"},
    {"huyr", "huy3", "hu\u1EF7"},
    {"luyx", "luy4", "lu\u1EF9"},
    {"quys", "quy1", "qu\u00FD"},
    {"quyr", "quy3", "qu\u1EF7"},
    {"thuys", "thuy1", "thu\u00FD"},
    {"duy", "duy", "duy"},
    {"tuy", "tuy", "tuy"},
    {"huy", "huy", "huy"},
    {"suy", "suy", "suy"},
    {"quy", "quy", "quy"},
    {"khuya", "khuya", "khuya"},
    {"chuyeejn", "chuye65n", "chuy\u1EC7n"},
    {"quyeen", "quye6n", "quy\u00EAn"},
    {"quyeesn", "quye61n", "quy\u1EBFn"},
    {"huyeejn", "huye65n", "huy\u1EC7n"},
    {"chuyeern", "chuye63n", "chuy\u1EC3n"},
    {"nguyeexn", "nguye64n", "nguy\u1EC5n"},
    {"nguyeen", "nguye6n", "nguy\u00EAn"},
    {"tuyeest", "tuye61t", "tuy\u1EBFt"},
    {"duyeejt", "duye65t", "duy\u1EC7t"},
    {"quyeest", "quye61t", "quy\u1EBFt"},
    {"thuyeefn", "thuye62n", "thuy\u1EC1n"},
    {"khuyru", "khuy3u", "khu\u1EF7u"},
    {"khuyfu", "khuy2u", "khu\u1EF3u"},
    {"tuw", "tu7", "t\u01B0"},
    {"thuw", "thu7", "th\u01B0"},
    {"chuwx", "chu74", "ch\u1EEF"},
    {"cuwr", "cu73", "c\u1EED"},
    {"huw", "hu7", "h\u01B0"},
    {"nuwx", "nu74", "n\u1EEF"},
    {"suw", "su7", "s\u01B0"},
    {"tuwf", "tu72", "t\u1EEB"},
    {"muwa", "mu7a", "m\u01B0a"},
    {"chuwa", "chu7a", "ch\u01B0a"},
    {"thuwa", "thu7a", "th\u01B0a"},
    {"dduwa", "d9u7a", "\u0111\u01B0a"},
    {"luwra", "lu73a", "l\u1EEDa"},
    {"cuwra", "cu73a", "c\u1EEDa"},
    {"giuwxa", "giu74a", "gi\u1EEFa"},
    {"nuwra", "nu73a", "n\u1EEDa"},
    {"chuwri", "chu73i", "ch\u1EEDi"},
    {"luwu", "lu7u", "l\u01B0u"},
    {"huwu", "hu7u", "h\u01B0u"},
    {"cuwsu", "cu71u", "c\u1EE9u"},
    {"muwu", "mu7u", "m\u01B0u"},
    {"huwxu", "hu74u", "h\u1EEFu"},
    {"tuowng", "tu7o7ng", "t\u01B0\u01A1ng"},
    {"truowfng", "tru7o72ng", "tr\u01B0\u1EDDng"},
    {"nguowfi", "ngu7o72i", "ng\u01B0\u1EDDi"},
    {"luowfi", "lu7o72i", "l\u01B0\u1EDDi"},
    {"huowng", "hu7o7ng", "h\u01B0\u01A1ng"},
    {"nuowsc", "nu7o71c", "n\u01B0\u1EDBc"},
    {"dduowjc", "d9u7o75c", "\u0111\u01B0\u1EE3c"},
    {"thuowng", "thu7o7ng", "th\u01B0\u01A1ng"},
    {"cuowfi", "cu7o72i", "c\u01B0\u1EDDi"},
    {"muowfi", "mu7o72i", "m\u01B0\u1EDDi"},
    {"dduowfng", "d9u7o72ng", "\u0111\u01B0\u1EDDng"},
    {"vuowfn", "vu7o72n", "v\u01B0\u1EDDn"},
    {"tuowsng", "tu7o71ng", "t\u01B0\u1EDBng"},
    {"phuowjng", "phu7o75ng", "ph\u01B0\u1EE3ng"},
    {"nguowxng", "ngu7o74ng", "ng\u01B0\u1EE1ng"},
    {"tuowi", "tu7o7i", "t\u01B0\u01A1i"},
    {"muowi", "mu7o7i", "m\u01B0\u01A1i"},
    {"ruowju", "ru7o75u", "r\u01B0\u1EE3u"},
    {"huowu", "hu7o7u", "h\u01B0\u01A1u"},
    {"khuowsu", "khu7o71u", "kh\u01B0\u1EDBu"},
    {"buowsu", "bu7o71u", "b\u01B0\u1EDBu"},
    {"y", "y", "y"},
    {"lys", "ly1", "l\u00FD"},
    {"kyx", "ky4", "k\u1EF9"},
    {"myx", "my4", "m\u1EF9"},
    {"tyr", "ty3", "t\u1EF7"},
    {"hy", "hy", "hy"},
    {"yeen", "ye6n", "y\u00EAn"},
    {"yeesu", "ye61u", "y\u1EBFu"},
    {"yeerm", "ye63m", "y\u1EC3m"},
    {"yeest", "ye61t", "y\u1EBFt"},
    {"yeeru", "ye63u", "y\u1EC3u"},
    {"gios", "gio1", "gi\u00F3"},
    {"giaf", "gia2", "gi\u00E0"},
    {"giuowfng", "giu7o72ng", "gi\u01B0\u1EDDng"},
    {"giuwx", "giu74", "gi\u1EEF"},
    {"giawjc", "gia85c", "gi\u1EB7c"},
    {"giaasy", "gia61y", "gi\u1EA5y"},
    {"giari", "gia3i", "gi\u1EA3i"},
    {"quaf", "qua2", "qu\u00E0"},
    {"quar", "qua3", "qu\u1EA3"},
    {"quas", "qua1", "qu\u00E1"},
    {"quan", "quan", "quan"},
    {"quaafn", "qua62n", "qu\u1EA7n"},
    {"quoosc", "quo61c", "qu\u1ED1c"},
    {"quee", "que6", "qu\u00EA"},
    {"nghe", "nghe", "nghe"},
    {"nghir", "nghi3", "ngh\u1EC9"},
    {"nghieeng", "nghie6ng", "nghi\u00EAng"},
    {"ngafy", "nga2y", "ng\u00E0y"},
    {"quyeefn", "quye62n", "quy\u1EC1n"},
};

nexkey::Options options_for(std::string_view method)
{
    nexkey::Options options;
    options.method = method == "telex" ? nexkey::Method::Telex : nexkey::Method::Vni;
    return options;
}

/// Type `keys` one byte at a time and return the committed text.
std::string type_all(const char* keys, const nexkey::Options& options)
{
    nexkey::Engine engine(options);
    std::string out;
    for (const char* key = keys; *key != '\0'; ++key) {
        out += engine.type(*key).commit;
    }
    out += engine.flush().commit;
    return out;
}

} // namespace

TEST(words_telex)
{
    for (const auto& word : kWords) {
        EXPECT_EQ(testing::render_word(word.telex, options_for("telex")),
                  std::string(word.text));
    }
}

TEST(words_vni)
{
    for (const auto& word : kWords) {
        EXPECT_EQ(testing::render_word(word.vni, options_for("vni")), std::string(word.text));
    }
}

TEST(words_type_step_by_step)
{
    // Typing key by key and committing at a boundary has to give the same text
    // as rendering the whole word at once, for both input methods.
    for (const auto& word : kWords) {
        EXPECT_EQ(type_all(word.telex, options_for("telex")), std::string(word.text));
        EXPECT_EQ(type_all(word.vni, options_for("vni")), std::string(word.text));
    }
}

TEST(words_state_equals_rendering_the_prefix)
{
    // A keystroke may only depend on the word typed so far, never on what came
    // before it: the preedit after n keys equals the render of the first n keys.
    for (const auto& word : kWords) {
        for (const std::string_view method : {"telex", "vni"}) {
            const char* keys = method == "telex" ? word.telex : word.vni;
            nexkey::Engine engine(options_for(method));
            for (std::size_t length = 1; keys[length - 1] != '\0'; ++length) {
                static_cast<void>(engine.type(keys[length - 1]));
                const std::string prefix(keys, length);
                EXPECT_EQ(std::string(engine.preedit()), engine.render(prefix));
            }
        }
    }
}

TEST(words_backspace_mirrors_typing)
{
    // Smart backspace keeps the raw keystrokes, so undoing a word has to pass
    // through the same visible states in reverse order.
    for (const auto& word : kWords) {
        const nexkey::Options options = options_for("telex");
        nexkey::Engine engine(options);
        std::vector<std::string> states;
        for (const char* key = word.telex; *key != '\0'; ++key) {
            static_cast<void>(engine.type(*key));
            states.push_back(std::string(engine.preedit()));
        }
        for (std::size_t index = states.size(); index-- > 0;) {
            EXPECT_EQ(std::string(engine.preedit()), states[index]);
            static_cast<void>(engine.backspace());
        }
        EXPECT_FALSE(engine.composing());
    }
}

TEST(words_commit_is_well_formed)
{
    for (const auto& word : kWords) {
        for (const std::string_view method : {"telex", "vni"}) {
            const char* keys = method == "telex" ? word.telex : word.vni;
            for (const nexkey::Encoding encoding :
                 {nexkey::Encoding::Unicode, nexkey::Encoding::UnicodeDecomposed}) {
                nexkey::Options options = options_for(method);
                options.encoding = encoding;
                nexkey::Engine engine(options);
                for (const char* key = keys; *key != '\0'; ++key) {
                    EXPECT_TRUE(nexkey::unicode::is_valid(engine.type(*key).commit));
                }
                const auto step = engine.flush();
                EXPECT_TRUE(nexkey::unicode::is_valid(step.commit));
            }
        }
    }
}

TEST(words_nfd_carries_the_same_text)
{
    // In NFD a letter is one to three code points, so a frontend that counts
    // code points would delete the wrong amount of text. This ties the two
    // facts together: the normalized NFD output is the same word, and it has
    // the same number of visible characters as the precomposed one.
    for (const auto& word : kWords) {
        nexkey::Options options = options_for("telex");
        options.encoding = nexkey::Encoding::UnicodeDecomposed;
        const std::string nfd = testing::render_word(word.telex, options);
        EXPECT_TRUE(nexkey::unicode::is_valid(nfd));
        EXPECT_EQ(nexkey::Converter::normalize(nfd, nexkey::Normalization::Nfc),
                  std::string(word.text));
        EXPECT_EQ(nexkey::unicode::count_graphemes(nfd),
                  nexkey::unicode::count_graphemes(word.text));
        EXPECT_TRUE(nexkey::Converter::equal_normalized(nfd, word.text));
    }
}

TEST(words_uppercase_first_letter)
{
    // Vietnamese documents start a sentence with a capital letter, and the
    // tone has to stay on the same vowel.
    EXPECT_EQ(testing::render_word("Tieengs", options_for("telex")), std::string("Ti\u1EBFng"));
    EXPECT_EQ(testing::render_word("Dduowngf", options_for("telex")),
              std::string("\u0110\u01B0\u1EDDng"));
    EXPECT_EQ(testing::render_word("Quyeets", options_for("telex")),
              std::string("Quy\u1EBFt"));
    EXPECT_EQ(testing::render_word("HOAF", options_for("telex")), std::string("HO\u00C0"));
}
