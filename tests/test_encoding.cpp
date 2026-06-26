#include "test_common.hpp"

#include "vkcore/VKEncoding.hpp"

void test_encoding()
{
    const auto tcvn3 = vkcore::VKEncoding::to_tcvn3("Tiếng Việt");
    EXPECT_FALSE(tcvn3.empty());

    const auto vni = vkcore::VKEncoding::to_vni("Tiếng Việt");
    EXPECT_FALSE(vni.empty());
}
