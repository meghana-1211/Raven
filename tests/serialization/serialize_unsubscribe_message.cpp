#include "test_serialization_utils.hpp"
#include <serialization/chunk.hpp>
#include <serialization/deserialization_impl.hpp>
#include <serialization/messages.hpp>
#include <serialization/serialization_impl.hpp>

using namespace rvn;
using namespace rvn::serialization;

void test1()
{
    depracated::messages:: UnsubscribeMessage msg;
    
    msg.subscribeId=271;
    ds::chunk c;
    detail::serialize(c, msg);

    // clang-format off
    // 
    // ( quic_msg_type: 0xa )  (msglen = 2) (subscribe id)                                            
    std::string expectedSerializationString = "[00001010] [00000010] [00000001 00001111] ";
    // clang-format on

    auto expectedSerialization = binary_string_to_vector(expectedSerializationString);
    utils::ASSERT_LOG_THROW(c.size() == expectedSerialization.size(), "Size mismatch\n",
                            "Expected size: ", expectedSerialization.size(),
                            "\n", "Actual size: ", c.size(), "\n");
    for (std::size_t i = 0; i < c.size(); i++)
        utils::ASSERT_LOG_THROW(c[i] == expectedSerialization[i], "Mismatch at index: ", i,
                                "\n", "Expected: ", expectedSerialization[i],
                                "\n", "Actual: ", c[i], "\n");
    ds::ChunkSpan span(c);


    depracated::messages::ControlMessageHeader header;
    detail::deserialize(header, span);

    utils::ASSERT_LOG_THROW(header.messageType_ ==
                            rvn::depracated::messages::MoQtMessageType::UNSUBSCRIBE,
                            "Header type mismatch\n", "Expected: ",
                            utils::to_underlying(rvn::depracated::messages::MoQtMessageType::UNSUBSCRIBE),
                            "\n", "Actual: ", utils::to_underlying(header.messageType_),
                            "\n");

    depracated::messages::UnsubscribeMessage deserialized;
    detail::deserialize(deserialized, span);

    utils::ASSERT_LOG_THROW(msg == deserialized, "Deserialization failed", "\n",
                            "Expected: ", msg, "\n", "Actual: ", deserialized, "\n");
}

void tests()
{
    try
    {
        test1();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Test failed\n";
        std::cerr << e.what() << std::endl;
    }
}


int main()
{
    tests();
    return 0;
}
