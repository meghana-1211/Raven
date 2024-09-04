#include <msquic.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <moqt.hpp>
#include <callbacks.hpp>

using namespace rvn;
const char *Target = "127.0.0.1";
const uint16_t UdpPort = 4567;
const uint64_t IdleTimeoutMs = 1000000000;

QUIC_CREDENTIAL_CONFIG *get_cred_config() {
    QUIC_CREDENTIAL_CONFIG *CredConfig =
        (QUIC_CREDENTIAL_CONFIG *)malloc(sizeof(QUIC_CREDENTIAL_CONFIG));

    memset(CredConfig, 0, sizeof(QUIC_CREDENTIAL_CONFIG));
    CredConfig->Type = QUIC_CREDENTIAL_TYPE_NONE;
    CredConfig->Flags = QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
    return CredConfig;
}

int main() {
    std::unique_ptr<MOQTClient> moqtClient = std::make_unique<MOQTClient>();

    QUIC_REGISTRATION_CONFIG RegConfig = {"quicsample", QUIC_EXECUTION_PROFILE_LOW_LATENCY};
    moqtClient->set_regConfig(&RegConfig);

    QUIC_SETTINGS Settings;
    std::memset(&Settings, 0, sizeof(Settings));

    Settings.IdleTimeoutMs = IdleTimeoutMs;
    Settings.IsSet.IdleTimeoutMs = TRUE;

    moqtClient->set_Settings(&Settings, sizeof(Settings));
    moqtClient->set_CredConfig(get_cred_config());
    QUIC_BUFFER AlpnBuffer = {sizeof("sample") - 1, (uint8_t *)"sample"};
    moqtClient->set_AlpnBuffers(&AlpnBuffer);

    moqtClient->set_AlpnBufferCount(1);

    moqtClient->set_connectionCb(rvn::callbacks::client_connection_callback);
    moqtClient->set_listenerCb(rvn::callbacks::client_listener_callback);

    moqtClient->set_controlStreamCb(rvn::callbacks::server_control_stream_callback);
    moqtClient->set_dataStreamCb(rvn::callbacks::server_data_stream_callback);


    moqtClient->start_connection(QUIC_ADDRESS_FAMILY_UNSPEC, Target, UdpPort);

    {
        char c;
        while (1)
            std::cin >> c;
    }
}