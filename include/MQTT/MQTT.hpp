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

#ifndef MQTT_HPP
#  define MQTT_HPP

#  include <mosquitto.h>
#  include <string>
#  include <vector>

// *****************************************************************************
//! \brief Base MQTT class based on the mosquitto implementation.
//! See https://github.com/eclipse/mosquitto
//! See https://www.howtoforge.com/how-to-install-mosquitto-mqtt-message-broker-on-debian-11/
// *****************************************************************************
class MQTT
{
public:

    enum class QoS { QoS0, QoS1, QoS2 };

    MQTT();
    virtual ~MQTT();
    bool connect(std::string const& addr, size_t const port);
    bool subscribe(std::string const& topic, QoS const qos);
    bool unsubscribe(std::string const& topic);
    bool publish(std::string const& topic, std::string const& payload, QoS const qos);
    bool publish(std::string const& topic, std::vector<char> const& payload, QoS const qos);
    bool publish(std::string const& topic, const char* payload, size_t const size, QoS const qos);

protected:

    struct mosquitto* mosquitto() { return m_mosquitto; }

private:

    virtual void onConnected(int /*rc*/) = 0;
    virtual void onDisconnected(int /*rc*/) {}
    virtual void onPublished(int /*mid*/) {}
    virtual void onSubscribed(int /*mid*/, int /*qos_count*/, const int */*granted_qos*/) {}
    virtual void onUnsubscribed(int /*mid*/) {}
    virtual void onMessageReceived(const struct mosquitto_message& message) = 0;

    static void on_connected_wrapper(struct mosquitto* /*mosqitto*/, void* userdata, int rc)
    {
        MQTT* m = static_cast<MQTT*>(userdata);
        m->onConnected(rc);
    }

    static void on_disconnected_wrapper(struct mosquitto* /*mosqitto*/, void* userdata, int rc)
    {
        MQTT* m = static_cast<MQTT*>(userdata);
        m->onDisconnected(rc);
    }

    static void on_published_wrapper(struct mosquitto* /*mosqitto*/, void* userdata, int mid)
    {
        MQTT* m = static_cast<MQTT*>(userdata);
        m->onPublished(mid);
    }

    static void on_subscribed_wrapper(struct mosquitto* /*mosqitto*/, void *userdata, int mid, int qos_count, const int *granted_qos)
    {
        MQTT* m = static_cast<MQTT*>(userdata);
        m->onSubscribed(mid, qos_count, granted_qos);
    }

    static void on_unsubscribed_wrapper(struct mosquitto* /*mosqitto*/, void *userdata, int mid)
    {
        MQTT* m = static_cast<MQTT*>(userdata);
        m->onUnsubscribed(mid);
    }

    static void on_message_received_wrapper(struct mosquitto* /*mosqitto*/, void *userdata, const struct mosquitto_message *message)
    {
        MQTT* m = static_cast<MQTT*>(userdata);
        m->onMessageReceived(*message);
    }

    class Mosquitto
    {
    public:

        static Mosquitto& instance() { static Mosquitto instance; return instance; }

    private:

        Mosquitto() { mosquitto_lib_init(); }
        ~Mosquitto() { mosquitto_lib_cleanup(); }
    };

private:

    struct mosquitto *m_mosquitto = nullptr;
};

#endif
