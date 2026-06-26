#include "test_common.hpp"

#include "vkcore/VKBuffer.hpp"

void test_telex()
{
    {
        vkcore::VKBuffer buf(vkcore::InputMethod::Telex, vkcore::Encoding::Unicode);
        buf.process_key('t');
        buf.process_key('a');
        buf.process_key('a');
        const auto r = buf.process_key(' ');
        EXPECT_EQ(r.committed, std::string("tâ "));
    }
    {
        vkcore::VKBuffer buf(vkcore::InputMethod::Telex, vkcore::Encoding::Unicode);
        for (char c : std::string("ans")) {
            buf.process_key(c);
        }
        const auto r = buf.process_key(' ');
        EXPECT_EQ(r.committed, std::string("án "));
    }
    {
        vkcore::VKBuffer buf(vkcore::InputMethod::Telex, vkcore::Encoding::Unicode);
        buf.process_key('d');
        buf.process_key('d');
        buf.process_key('i');
        const auto r = buf.process_key(' ');
        EXPECT_EQ(r.committed, std::string("đi "));
    }
    {
        vkcore::VKBuffer buf(vkcore::InputMethod::Telex, vkcore::Encoding::Unicode);
        for (char c : std::string("tieengs")) {
            buf.process_key(c);
        }
        const auto r = buf.process_key(' ');
        EXPECT_EQ(r.committed, std::string("tiếng "));
    }
}
