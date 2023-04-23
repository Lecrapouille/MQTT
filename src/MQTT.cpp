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

//-----------------------------------------------------------------------------
MQTT::MQTT()
{
    MQTT::Mosquitto::instance();
}

//-----------------------------------------------------------------------------
MQTT::~MQTT()
{
    if (m_mosquitto != nullptr)
    {
        mosquitto_destroy(m_mosquitto);
    }
}

//-----------------------------------------------------------------------------
bool MQTT::connect(std::string const& addr, size_t const port)
{
    if (m_mosquitto == nullptr)
    {
        m_mosquitto = mosquitto_new(nullptr, true, this);
        if (m_mosquitto == nullptr)
        {
            std::cerr << "MQTT Error: cannot malloc mosquitto" << std::endl;
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

    int rc = mosquitto_connect(m_mosquitto, addr.c_str(), int(port), 60);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "MQTT connection error: " << mosquitto_strerror(rc) << std::endl;
        return false;
    }

    rc = mosquitto_loop_start(m_mosquitto);
    if (rc != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "MQTT loop_start error: " << mosquitto_strerror(rc) << std::endl;
        return false;
    }

    return true;
}

//-----------------------------------------------------------------------------
bool MQTT::publish(std::string const& topic, const char* payload, size_t const size, QoS const qos)
{
    int rc = mosquitto_publish(m_mosquitto, nullptr, topic.c_str(), size, payload, int(qos), false);
    if( rc != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "MQTT Error publishing: " << mosquitto_strerror(rc)
                  << std::endl;
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool MQTT::publish(std::string const& topic, std::vector<char> const& payload, QoS const qos)
{
    int rc = mosquitto_publish(m_mosquitto, nullptr, topic.c_str(),
                               payload.size(), payload.data(), int(qos), false);
    if( rc != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "MQTT Error publishing: " << mosquitto_strerror(rc)
                  << std::endl;
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool MQTT::publish(std::string const& topic, std::string const& payload, QoS const qos)
{
    int rc = mosquitto_publish(m_mosquitto, nullptr, topic.c_str(),
                               payload.size(), payload.data(), int(qos), false);
    if( rc != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "MQTT Error publishing: " << mosquitto_strerror(rc)
                  << std::endl;
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool MQTT::unsubscribe(std::string const& topic)
{
    if (topic.size() == 0u)
        return true;

    std::cout << "MQTT unsubscribe to '" << topic << "'" << std::endl;
    int rc = mosquitto_unsubscribe(m_mosquitto, nullptr, topic.c_str());
    if (rc != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "MQTT Error unsubscribing: " << mosquitto_strerror(rc)
                  << std::endl;
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool MQTT::subscribe(std::string const& topic, QoS const qos)
{
    std::cout << "MQTT subscribe to '" << topic << "'" << std::endl;
    int rc = mosquitto_subscribe(m_mosquitto, nullptr, topic.c_str(), int(qos));
    if (rc != MOSQ_ERR_SUCCESS)
    {
        std::cerr << "MQTT Error subscribing: " << mosquitto_strerror(rc)
                  << std::endl;
        return false;
    }
    return true;
}
