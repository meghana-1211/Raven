#include "setup.hpp"

int main()
{
    /*
        Server will be out parent process and client will be child process.
    */
    if (auto childPid = fork())
    {
        // parent process, server
        std::unique_ptr<MOQTServer> moqtServer = server_setup();
        moqtServer->register_object("tnamespace", "tname", 0, 0, data);

        using namespace std::chrono_literals;
        // Make sure server is alive until client is setup and requests
        std::this_thread::sleep_for(1.5s);
    }
    else
    {
        // child process
        std::unique_ptr<MOQTClient> moqtClient = client_setup();

        SubscriptionBuilder subscriptionBuilder;
        subscriptionBuilder.set_data_range<SubscriptionBuilder::Filter::LatestGroup>(
        std::size_t(0));
        subscriptionBuilder.set_track_alias(0);
        subscriptionBuilder.set_track_namespace("tnamespace");
        subscriptionBuilder.set_track_name("tname");
        subscriptionBuilder.set_subscriber_priority(0);
        subscriptionBuilder.set_group_order(0);

        auto subMessage = subscriptionBuilder.build();

        using namespace std::chrono_literals;
        // Make sure `register_object` is called before `subscribe`
        auto queueRef = moqtClient->subscribe(std::move(subMessage));

        std::string receivedData;

        queueRef->wait_dequeue(receivedData);
        std::cout << "Received data: " << receivedData << std::endl;
        assert(receivedData == data);
    }
}
