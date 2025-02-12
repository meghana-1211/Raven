#pragma once
//////////////////////////////
#include <data_manager.hpp>
#include <serialization/messages.hpp>
#include <strong_types.hpp>
//////////////////////////////
#include <functional>
#include <memory>
#include <optional>
//////////////////////////////
#include <definitions.hpp>
#include <deserializer.hpp>
#include <message_handler.hpp>
#include <serialization/serialization.hpp>
#include <utilities.hpp>
#include <wrappers.hpp>
//////////////////////////////

extern "C"
{
#include <msquic.h>
}

namespace rvn
{
enum class StreamType
{
    CONTROL,
    DATA
};

struct StreamContext
{
    std::atomic_bool streamHasBeenConstructed{};
    class MOQT& moqtObject_;
    /*  We can not have reference to StreamState
        StreamState constructor takes rvn::unique_stream which requires
        StreamContext to be constructed
        Constructors can not have cyclical dependency
     */
    struct ConnectionState& connectionState_;
    /*
        We can not have operator= for Deserializer because it contains
        a non movable object (std::mutex)
        We can not construct it in constructor because it requires
        StreamState which requires rvn::unique_stream which requires StreamContext
    */
    std::optional<serialization::Deserializer<MessageHandler>> deserializer_;
    StreamContext(MOQT& moqtObject, ConnectionState& connectionState)
    : moqtObject_(moqtObject), connectionState_(connectionState) {};

    // deserializer can not be constructed in the constructor and has to be done seperately
    void construct_deserializer(StreamState& streamState, bool isControlStream);
};

class StreamSendContext
{
public:
    // owns the QUIC_BUFFERS
    QUIC_BUFFER* buffer;
    std::uint32_t bufferCount;

    // non owning reference
    const StreamContext* streamContext;

    std::function<void(StreamSendContext*)> sendCompleteCallback =
    utils::NoOpVoid<StreamSendContext*>;

    StreamSendContext(QUIC_BUFFER* buffer_,
                      const std::uint32_t bufferCount_,
                      const StreamContext* streamContext_,
                      std::function<void(StreamSendContext*)> sendCompleteCallback_ =
                      utils::NoOpVoid<StreamSendContext*>)
    : buffer(buffer_), bufferCount(bufferCount_), streamContext(streamContext_),
      sendCompleteCallback(sendCompleteCallback_)
    {
        utils::ASSERT_LOG_THROW(bufferCount == 1, "bufferCount should be 1", bufferCount);
    }

    ~StreamSendContext()
    {
        destroy_buffers();
    }

    std::tuple<QUIC_BUFFER*, std::uint32_t> get_buffers()
    {
        return { buffer, bufferCount };
    }
    void destroy_buffers()
    {
        //  bufferCount is always 1
        // the way we allocate buffers is  malloc(sizeof(QUIC_BUFFER)) and this
        // becomes our (QUIC_BUFFER *) buffers
        free(buffer);
        bufferCount = 0;
    }

    // callback called when the send is succsfull
    void send_complete_cb()
    {
        sendCompleteCallback(this);
    }
};

struct StreamState
{
    rvn::unique_stream stream;
    struct ConnectionState& connectionState_;
    std::unique_ptr<StreamContext> streamContext{};

    StreamState(rvn::unique_stream&& stream_, struct ConnectionState& connectionState_)
    : stream(std::move(stream_)), connectionState_(connectionState_)
    {
    }

    void set_stream_context(std::unique_ptr<StreamContext> streamContext_)
    {
        this->streamContext = std::move(streamContext_);
    }
};

class DataStreamState : public StreamState, public std::enable_shared_from_this<DataStreamState>
{
    // return weak_ptr to this for 3rd party to observe lifetime of the StreamState
    std::shared_ptr<void> lifeTimeFlag_;

public:
    // To be used by subscriber to receive objects on this stream
    std::shared_ptr<MPMCQueue<StreamHeaderSubgroupObject>> objectQueue_;
    std::shared_ptr<StreamHeaderSubgroupMessage> streamHeaderSubgroupMessage_;

    DataStreamState(rvn::unique_stream&& stream, struct ConnectionState& connectionState);
    bool can_send_object(const ObjectIdentifier& objectIdentifier) const noexcept;
    void set_header(StreamHeaderSubgroupMessage streamHeaderSubgroupMessage);
    std::weak_ptr<void> get_life_time_flag() const noexcept;
};

struct ConnectionState : std::enable_shared_from_this<ConnectionState>
{
    // StreamManager //////////////////////////////////////////////////////////////
    std::shared_mutex trackAliasMtx_;
    std::unordered_map<TrackIdentifier, TrackAlias, TrackIdentifier::Hash, TrackIdentifier::Equal> trackAliasMap_;
    std::unordered_map<std::uint64_t, TrackIdentifier> trackAliasRevMap_;

    void add_track_alias(TrackIdentifier trackIdentifier, TrackAlias trackAlias);

    // wtf is currGroup?
    std::shared_mutex currGroupMtx_;
    std::unordered_map<TrackIdentifier, GroupId, TrackIdentifier::Hash, TrackIdentifier::Equal> currGroupMap_;
    std::optional<GroupId> get_current_group(const TrackIdentifier& trackIdentifier);
    std::optional<GroupId> get_current_group(TrackAlias trackAlias);

    std::optional<TrackIdentifier> alias_to_identifier(TrackAlias trackAlias);
    std::optional<TrackAlias> identifier_to_alias(const TrackIdentifier& trackIdentifier);

    StableContainer<DataStreamState> dataStreams;

    std::optional<StreamState> controlStream{};

    void delete_data_stream(HQUIC streamHandle);
    void enqueue_data_buffer(QUIC_BUFFER* buffer);

    QUIC_STATUS send_object(std::weak_ptr<DataStreamState> dataStream,
                            const ObjectIdentifier& objectIdentifier,
                            QUIC_BUFFER* buffer);
    QUIC_STATUS send_object(const ObjectIdentifier& objectIdentifier, QUIC_BUFFER* buffer);
    void send_control_buffer(QUIC_BUFFER* buffer, QUIC_SEND_FLAGS flags = QUIC_SEND_FLAG_NONE);
    /////////////////////////////////////////////////////////////////////////////

    unique_connection connection_;
    MOQT& moqtObject_;

    std::string path;
    // TODO: role

    ConnectionState(unique_connection&& connection, class MOQT& moqtObject)
    : connection_(std::move(connection)), moqtObject_(moqtObject)
    {
    }

    const StableContainer<DataStreamState>& get_data_streams() const;
    StableContainer<DataStreamState>& get_data_streams();
    std::optional<StreamState>& get_control_stream();
    const std::optional<StreamState>& get_control_stream() const;


    QUIC_STATUS accept_data_stream(HQUIC dataStreamHandle);

    QUIC_STATUS accept_control_stream(HQUIC controlStreamHandle);

    StreamState& establish_control_stream();
};

} // namespace rvn
