#include "test_serialization_utils.hpp"
#include <serialization/chunk.hpp>
#include <serialization/deserialization_impl.hpp>
#include <serialization/messages.hpp>
#include <serialization/serialization_impl.hpp>

using namespace rvn;
using namespace rvn::serialization;

void test1()
{
    depracated::messages::AnnounceErrorMessage msg;
    msg.errorCode_=12;
    msg.trackNamespace_= "0101001010101101";
    msg.reasonPhrase_= "1100000000010000010000000000100010000111011001010100001100100001";
    ds::chunk c;
    detail::serialize(c, msg);

    // clang-format off
    //  [ 01000000 00001000 ]      00001110          00001100            
    //                                       ( quic_msg_type: 0x8 )(msglen = 11)  (trackNamespace)  (errorCode)                 (reasonPhrase)                                                             
    std::string expectedSerializationString = "[00000000 000001000] [00001011] [01010010 10101101] [00001100] [11000000 00010000 01000000 00001000 10000111 01100101 01000011 00100001] ";
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
                            rvn::depracated::messages::MoQtMessageType::ANNOUNCE_ERROR,
                            "Header type mismatch\n", "Expected: ",
                            utils::to_underlying(rvn::depracated::messages::MoQtMessageType::ANNOUNCE_ERROR),
                            "\n", "Actual: ", utils::to_underlying(header.messageType_),
                            "\n");

    depracated::messages::AnnounceErrorMessage deserialized;
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
