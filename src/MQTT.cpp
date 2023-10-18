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
#include <cstring>

//-----------------------------------------------------------------------------
void MQTT::libMosquittoInit()
{
    size_t& counter = libMosquittoCountInstances();
    if (counter == 0u) {
        // std::cout << "Call mosquitto_lib_init" << std::endl;
        mosquitto_lib_init();
    }
    counter += 1u;
}

//-----------------------------------------------------------------------------
void MQTT::libMosquittoCleanUp()
{
    size_t& counter = libMosquittoCountInstances();
    assert((counter != 0u) && "invalid counter");

    counter -= 1u;
    if (counter == 0u) {
        // std::cout << "Call mosquitto_lib_cleanup" << std::endl;
        mosquitto_lib_cleanup();
    }
}

//-----------------------------------------------------------------------------
size_t MQTT::Message::store(std::vector<uint8_t>& payload, bool const clear)
{
    if (clear) {
        payload.clear();
    }
    payload.resize(payload.size() + this->payloadlen);
    std::memcpy(&payload[payload.size() - this->payloadlen], this->payload, this->payloadlen * sizeof(uint8_t));
    return payload.size();
}

//-----------------------------------------------------------------------------
MQTT::MQTT()
{
    libMosquittoInit();
}

//-----------------------------------------------------------------------------
MQTT::~MQTT()
{
    if (m_mosquitto != nullptr) {
        mosquitto_destroy(m_mosquitto);
    }
    libMosquittoCleanUp();
}

//-----------------------------------------------------------------------------
bool MQTT::connect(std::string const& client_id, std::string const& addr, size_t const port)
{
    if ((client_id.size() == 0u) || (client_id.size() > 23u))
    {
        m_error = "Invalid number of char defining the client ID";
        return false;
    }
    return doConnection(client_id.c_str(), addr.c_str(), port);
}

//-----------------------------------------------------------------------------
bool MQTT::connect(std::string const& addr, size_t const port)
{
    return doConnection(nullptr, addr.c_str(), port);
}

//-----------------------------------------------------------------------------
bool MQTT::doConnection(const char* client_id, const char* addr, size_t const port)
{
    if (m_mosquitto == nullptr)
    {
        m_mosquitto = mosquitto_new(client_id, true, this);
        if (m_mosquitto == nullptr)
        {
            m_error = "MQTT Error: cannot malloc mosquitto";
            return false;
        }
    }
    else
    {
        mosquitto_disconnect(m_mosquitto);
    }

    mosquitto_connect_callback_set(m_mosquitto, on_connected_wrapper);
    mosquitto_disconnect_callback_set(m_mosquitto, on_disconnected_wrapper);
    mosquitto_publish_callback_set(m_mosquitto, on_published_wrapper);
    mosquitto_subscribe_callback_set(m_mosquitto, on_subscribed_wrapper);
    mosquitto_unsubscribe_callback_set(m_mosquitto, on_unsubscribed_wrapper);
    mosquitto_message_callback_set(m_mosquitto, on_message_received_wrapper);

    int rc = mosquitto_connect(m_mosquitto, addr, int(port), 60);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_error = mosquitto_strerror(rc);
        return false;
    }

    rc = mosquitto_loop_start(m_mosquitto);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_error = mosquitto_strerror(rc);
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool MQTT::publish(std::string const& topic, const uint8_t* payload, size_t const size, QoS const qos)
{
    assert((topic.size() != 0u) && "topic name shall not be empty");

    int rc = mosquitto_publish(m_mosquitto, nullptr, topic.c_str(), size, payload, int(qos), false);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_error = mosquitto_strerror(rc);
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool MQTT::publish(std::string const& topic, std::vector<uint8_t> const& payload, QoS const qos)
{
    assert((topic.size() != 0u) && "topic name shall not be empty");

    int rc = mosquitto_publish(m_mosquitto, nullptr, topic.c_str(),
                               payload.size(), payload.data(), int(qos), false);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_error = mosquitto_strerror(rc);
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool MQTT::publish(std::string const& topic, std::string const& payload, QoS const qos)
{
    assert((topic.size() != 0u) && "topic name shall not be empty");

    // We exploit the fact that mosquitto appends an extra 0 bytes that makes
    // ad direct reading as a C string.
    int rc = mosquitto_publish(m_mosquitto, nullptr, topic.c_str(),
                               payload.size(), payload.data(), int(qos), false);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_error = mosquitto_strerror(rc);
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool MQTT::unsubscribe(std::string const& topic)
{
    assert((topic.size() != 0u) && "topic name shall not be empty");

    int rc = mosquitto_unsubscribe(m_mosquitto, nullptr, topic.c_str());
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_error = mosquitto_strerror(rc);
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool MQTT::subscribe(std::string const& topic, QoS const qos)
{
    assert((topic.size() != 0u) && "topic name shall not be empty");

    int rc = mosquitto_subscribe(m_mosquitto, nullptr, topic.c_str(), int(qos));
    if (rc != MOSQ_ERR_SUCCESS)
    {
        m_error = mosquitto_strerror(rc);
        return false;
    }
    return true;
}
