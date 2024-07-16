//*****************************************************************************
// A C++ class wrapping Mosquitto MQTT https://github.com/eclipse/mosquitto
//
// MIT License
//
// Copyright (c) 2023 Quentin Quadrat <lecrapouille@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//*****************************************************************************

#include "MQTT/MQTT.hpp"
#include <iostream>

namespace mqtt {

// *************************************************************************
//! \brief std::error_code instead of throw() or errno.
// *************************************************************************
struct MQTTErrorCategory : std::error_category
{
    virtual const char* name() const noexcept override { return "MQTT"; }
    virtual std::string message(int ec) const override
    {
        if (!custom_message.empty())
            return custom_message;
        return mosquitto_strerror(ec);
    }

    std::string custom_message;
};

// -----------------------------------------------------------------------------
static MQTTErrorCategory s_mqtt_error_category;

// -----------------------------------------------------------------------------
static std::error_code make_error_code(int ec)
{
    return { ec, s_mqtt_error_category };
}

// -----------------------------------------------------------------------------
static std::error_code make_error_code(int ec, std::string const& message)
{
    s_mqtt_error_category.custom_message = message;
    return { ec, s_mqtt_error_category };
}

//-----------------------------------------------------------------------------
bool Client::libMosquittoInit(Protocol protocol)
{
    size_t& counter = libMosquittoCountInstances();
    if (++counter > 1u)
        return true;

    //std::cout << "Call mosquitto_lib_init" << std::endl;
    int rc = mosquitto_lib_init();
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_mosquitto = nullptr;
        m_status = Client::Status::InDefect;
        m_error = make_error_code(rc);
        return false;
    }

    mosquitto_lib_version(&m_version.mosquitto[0],
                          &m_version.mosquitto[1],
                          &m_version.mosquitto[2]);
    switch (protocol)
    {
    case Protocol::V31:
        m_version.protocol[0] = 3;
        m_version.protocol[1] = 1;
        m_version.protocol[2] = 0;
        break;
    case Protocol::V311:
        m_version.protocol[0] = 3;
        m_version.protocol[1] = 1;
        m_version.protocol[2] = 1;
        break;
    case Protocol::V5:
        m_version.protocol[0] = 5;
        m_version.protocol[1] = 0;
        m_version.protocol[2] = 0;
        break;
    }

    mosquitto_int_option(m_mosquitto, MOSQ_OPT_PROTOCOL_VERSION, int(protocol));
    return true;
}

//-----------------------------------------------------------------------------
bool Client::instantiate(char const* client_id, const bool clean_session)
{
    m_mosquitto = mosquitto_new(client_id, clean_session, this);
    if (m_mosquitto == nullptr)
    {
        m_status = Client::Status::InDefect;
        m_error = make_error_code(
            MOSQ_ERR_NOMEM, "MQTT Error: cannot malloc mosquitto");
        return false;
    }

    mosquitto_connect_callback_set(m_mosquitto, on_connected_wrapper);
    mosquitto_disconnect_callback_set(m_mosquitto, on_disconnected_wrapper);
    mosquitto_publish_callback_set(m_mosquitto, on_published_wrapper);
    mosquitto_subscribe_callback_set(m_mosquitto, on_subscribed_wrapper);
    mosquitto_unsubscribe_callback_set(m_mosquitto, on_unsubscribed_wrapper);
    mosquitto_message_callback_set(m_mosquitto, on_message_received_wrapper);
    return true;
}

//-----------------------------------------------------------------------------
void Client::libMosquittoCleanUp()
{
    size_t& counter = libMosquittoCountInstances();
    if (--counter == 0u)
    {
        //std::cout << "Call mosquitto_lib_cleanup" << std::endl;
        mosquitto_lib_cleanup();
    }
}

//-----------------------------------------------------------------------------
Client::Client(Client::Settings const& settings)
{
    if (libMosquittoInit(settings.protocol))
    {
        instantiate(settings.client_id.size() == 0u ? nullptr :
                    settings.client_id.c_str(),
                    settings.session == Client::Session::Cleanup);
    }
}

//-----------------------------------------------------------------------------
Client::~Client()
{
    if (m_mosquitto != nullptr)
    {
        mosquitto_disconnect(m_mosquitto);
        mosquitto_destroy(m_mosquitto);
    }
    libMosquittoCleanUp();
}

//-----------------------------------------------------------------------------
bool Client::disconnect()
{
    if (m_mosquitto == nullptr)
        return false;

    int rc = mosquitto_disconnect(m_mosquitto);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_error = make_error_code(rc);
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool Client::connect(
    Connection const settings, Client::ConnectionCallback onConnected,
    Client::ConnectionCallback onDisconnected)
{
    if (m_mosquitto == nullptr)
        return false;

    if (m_status == Client::Status::Connected)
        return true;

    m_callbacks.connection = onConnected;
    m_callbacks.disconnection = onDisconnected;
    int rc = mosquitto_connect(
        m_mosquitto, settings.address.c_str(), int(settings.port),
        int(settings.timeout.count()));
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_error = make_error_code(rc);
        return false;
    }

    rc = mosquitto_loop_start(m_mosquitto);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_error = make_error_code(rc);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
void Client::on_connected_wrapper(struct mosquitto*, void* userdata, int rc)
{
    Client* client = static_cast<Client*>(userdata);
    assert((client != nullptr) && "NULL pointer passed as param");
    client->m_status = Client::Status::Connected;
    client->m_callbacks.reception.clear();
    if (client->m_callbacks.connection != nullptr)
    {
        client->m_callbacks.connection(rc);
    }
    else
    {
        client->onConnected(rc);
    }
}

//-----------------------------------------------------------------------------
void Client::on_disconnected_wrapper(struct mosquitto*, void* userdata, int rc)
{
    Client* client = static_cast<Client*>(userdata);
    assert((client != nullptr) && "NULL pointer passed as param");
    client->m_status = Client::Status::Disconnected;
    if (client->m_callbacks.disconnection != nullptr)
    {
        client->m_callbacks.disconnection(rc);
    }
    else
    {
        client->onDisconnected(rc);
    }
    client->m_callbacks.connection = nullptr;
    client->m_callbacks.disconnection = nullptr;
    client->m_callbacks.reception.clear();
}

//-----------------------------------------------------------------------------
bool Client::publish(Topic& topic, const uint8_t* payload,
    size_t const size, QoS const qos)
{
    if (topic.name.size() == 0u)
    {
        m_error = make_error_code(
            MOSQ_ERR_INVAL, "topic name shall not be empty");
        return false;
    }

    if ((payload == nullptr) && (size != 0u))
    {
        m_error = make_error_code(
            MOSQ_ERR_INVAL, "invalid payload content or payload size");
        return false;
    }

    int rc = mosquitto_publish(
        m_mosquitto, &topic.id, topic.name.c_str(), int(size), payload,
        int(qos), topic.retain);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_error = make_error_code(rc);
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool Client::publish(Topic& topic, std::vector<uint8_t> const& payload, QoS const qos)
{
    return publish(topic, payload.data(), payload.size(), qos);
}

//-----------------------------------------------------------------------------
bool Client::publish(Topic& topic, std::string const& payload, QoS const qos)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&payload[0]);
    return publish(topic, p, payload.size() + 1u, qos);
}

//-----------------------------------------------------------------------------
bool Client::unsubscribe(Topic& topic)
{
    int rc = mosquitto_unsubscribe(m_mosquitto, &topic.id, topic.name.c_str());
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_error = make_error_code(rc);
        return false;
    }

    m_callbacks.reception.erase(topic.name);
    return true;
}

//-----------------------------------------------------------------------------
bool Client::subscribe(Topic& topic, QoS const qos,
    Client::ReceptionCallback onMessageReceived)
{
    if (topic.name.size() == 0u)
    {
        m_error = make_error_code(MOSQ_ERR_INVAL, "topic name shall not be empty");
        return false;
    }

    int rc = mosquitto_subscribe(m_mosquitto, &topic.id, topic.name.c_str(), int(qos));
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_error = make_error_code(rc);
        return false;
    }

    m_callbacks.reception[topic.name] = onMessageReceived;
    return true;
}

//-----------------------------------------------------------------------------
void Client::on_message_received_wrapper(
    struct mosquitto*, void *userdata, const struct mosquitto_message *msg)
{
    Client* client = static_cast<Client*>(userdata);
    assert((client != nullptr) && "NULL pointer passed as param");
    Message const& message = *reinterpret_cast<const Message*>(msg);

    auto const& callback = client->m_callbacks.reception.find(message.topic);
    if ((callback != client->m_callbacks.reception.end()) &&
        (callback->second != nullptr))
    {
        callback->second(message);
    }
    else
    {
        client->onMessageReceived(message);
    }
}

} // namespace mqtt