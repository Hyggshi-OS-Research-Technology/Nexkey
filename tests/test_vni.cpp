#include "test_common.hpp"

#include "vkcore/VKBuffer.hpp"

void test_vni()
{
    vkcore::VKBuffer buf(vkcore::InputMethod::VNI, vkcore::Encoding::Unicode);
    for (char c : std::string("tie6ng1")) {
        buf.process_key(c);
    }
    const auto r = buf.process_key(' ');
    EXPECT_EQ(r.committed, std::string("tiếng "));
}
