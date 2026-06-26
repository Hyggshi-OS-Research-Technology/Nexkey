#include "test_common.hpp"

#include "vkcore/VKBuffer.hpp"

int main()
{
    test_telex();
    test_vni();
    test_encoding();
    test_buffer();
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

void test_buffer()
{
    vkcore::VKBuffer buf(vkcore::InputMethod::Telex, vkcore::Encoding::Unicode);
    for (char c : std::string("ans")) {
        buf.process_key(c);
    }
    buf.process_key(0, true);
    const auto r = buf.process_key(' ');
    EXPECT_EQ(r.committed, std::string("an "));
}
