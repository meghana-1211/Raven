#pragma once

#include <moqt.hpp>
#include <serialization.hpp>


namespace rvn
{
template <typename MOQTObject> class MessageHandler
{
    MOQTObject& moqt;
    ConnectionState& connectionState;

    QUIC_STATUS handle_message(protobuf_messages::ClientSetupMessage&& clientSetupMessage)
    {

        auto& supportedversions = clientSetupMessage.supportedversions();
        auto matchingVersionIter =
        std::find(supportedversions.begin(), supportedversions.end(), moqt.version);

        if (matchingVersionIter == supportedversions.end())
        {
            // TODO
            // destroy connection
            // connectionState.destroy_connection();
            return QUIC_STATUS_INVALID_PARAMETER;
        }

        std::size_t iterIdx = std::distance(supportedversions.begin(), matchingVersionIter);
        auto& params = clientSetupMessage.parameters()[iterIdx];
        connectionState.path = std::move(params.path().path());
        connectionState.peerRole = params.role().role();

        utils::LOG_EVENT(std::cout, "Client Setup Message received: \n",
                         clientSetupMessage.DebugString());

        // send SERVER_SETUP message
        protobuf_messages::MessageHeader serverSetupHeader;
        serverSetupHeader.set_messagetype(protobuf_messages::MoQtMessageType::SERVER_SETUP);

        protobuf_messages::ServerSetupMessage serverSetupMessage;
        serverSetupMessage.add_parameters()->mutable_role()->set_role(
        protobuf_messages::Role::Publisher);

        QUIC_BUFFER* quicBuffer =
        serialization::serialize(serverSetupHeader, serverSetupMessage);

        connectionState.streamManager->enqueue_control_buffer(quicBuffer);

        return QUIC_STATUS_SUCCESS;
    }

    QUIC_STATUS handle_message(protobuf_messages::ServerSetupMessage&& serverSetupMessage)
    {

        utils::ASSERT_LOG_THROW(connectionState.path.size() == 0,
                                "Server must now use the path "
                                "parameter");
        utils::ASSERT_LOG_THROW(serverSetupMessage.parameters().size() > 0,
                                "SERVER_SETUP sent no parameters, "
                                "requires atleast role parameter");

        utils::LOG_EVENT(std::cout, "Server Setup Message received: ",
                         serverSetupMessage.DebugString());

        connectionState.peerRole = serverSetupMessage.parameters()[0].role().role();

        moqt.subscribe();

        return QUIC_STATUS_SUCCESS;
    }

    QUIC_STATUS handle_message(protobuf_messages::SubscribeMessage&& subscribeMessage)
    {
        // Subscriber sends to Publisher

        utils::ASSERT_LOG_THROW(connectionState.peerRole == protobuf_messages::Role::Publisher,
                                "Client can only subscribe to Publisher");

        utils::ASSERT_LOG_THROW(subscribeMessage.has_startgroup() ^
                                subscribeMessage.has_startobject(),
                                "StartGroup and StartObject must both be sent "
                                "or both should not be sent");

        utils::ASSERT_LOG_THROW(subscribeMessage.has_startgroup() &&
                                (subscribeMessage.filtertype() == protobuf_messages::AbsoluteStart ||
                                 subscribeMessage.filtertype() == protobuf_messages::AbsoluteRange),
                                "Start group Id is only present for "
                                "AbsoluteStart and AbsoluteRange filter "
                                "types.");

        utils::ASSERT_LOG_THROW(!subscribeMessage.has_startobject() &&
                                (subscribeMessage.filtertype() == protobuf_messages::AbsoluteStart ||
                                 subscribeMessage.filtertype() == protobuf_messages::AbsoluteRange),
                                "Start object Id is only present for "
                                "AbsoluteStart and AbsoluteRange filter "
                                "types.");

        utils::ASSERT_LOG_THROW(subscribeMessage.has_endgroup() ^
                                subscribeMessage.has_endobject(),
                                "EndGroup and EndObject must both be sent or "
                                "both should not be sent");

        utils::ASSERT_LOG_THROW(subscribeMessage.has_endgroup() &&
                                (subscribeMessage.filtertype() == protobuf_messages::AbsoluteRange),
                                "End group Id is only present for "
                                "AbsoluteRange filter types.");

        // EndObject: The end Object ID, plus 1. A value of 0
        // means the entire group is
        // requested.
        utils::ASSERT_LOG_THROW(!subscribeMessage.has_endobject() &&
                                (subscribeMessage.filtertype() == protobuf_messages::AbsoluteRange),
                                "End object Id is only present for "
                                "AbsoluteRange filter types.");

        auto [payloadStream, errorInfo] =
        moqt.get_payload_ss(connectionState, subscribeMessage);
        if (errorInfo.errc != 0)
        {
            // TODO: error handling and sending
            // SUBSCRIBE_ERROR message
            // send_subscribe_error(connectionState,
            // subscribeMessage, errorInfo.value());
        }

        // TODO: ordering between subscribe ok and register
        // subscription (which manages sending of data)
        utils::LOG_EVENT(std::cout, "Subscribe Message received: \n",
                         subscribeMessage.DebugString());

        connectionState.register_subscription(subscribeMessage, payloadStream);
        // send_subscription_ok(connectionState, subscribeMessage);

        return QUIC_STATUS_SUCCESS;
    }


public:
    MessageHandler(MOQTObject& moqt, ConnectionState& connectionState)
    : moqt(moqt), connectionState(connectionState)
    {
    }


    template <typename MessageType>
    QUIC_STATUS handle_message(google::protobuf::io::IstreamInputStream& istream)
    {
        MessageType message = serialization::deserialize<MessageType>(istream);

        return handle_message(std::move(message));
    }
};
} // namespace rvn