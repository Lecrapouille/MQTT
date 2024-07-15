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
#  include <cstring>
#  include <vector>
#  include <cassert>
#  include <map>
#  include <atomic>
#  include <functional>
#  include <chrono>
#  include <system_error>

// ****************************************************************************
//! \brief Base asynchronous MQTT v3 class based on the mosquitto implementation.
//! This version is not thread safe.
//!
//! You shall inherit from this class and implements the following private
//! callbacks:
//!   - onConnected() from where you have to call subscribe(topic, qos) for
//!     subscribing to desired topics.
//!   - onMessageReceived(const struct mosquitto_message& message) when a new
//!     message has been received.
//!
//! The following callbacks does nothing, you have to override them to implement
//! a behavior:
//!   - void onDisconnected(int rc)
//!   - void onPublished(int mid)
//!   -  void onSubscribed(int mid, int qos_count, const int *granted_qos)
//!   -  void onUnsubscribed(int mid)
//!
//! Mostly of this API returns false in case of falure and true in case of
//! success. In this case call error() to get a human readable message.
//!
//! Since this header includes the mosquitto header, you can access to C fucntions
//! https://mosquitto.org/api/files/mosquitto-h.html
//!
//! References:
//!   - https://github.com/eclipse/mosquitto
//!   - https://www.howtoforge.com/how-to-install-mosquitto-mqtt-message-broker-on-debian-11/
// ****************************************************************************
class MQTT
{
public:

    //-------------------------------------------------------------------------
    //! \brief MQTT Protocol version (3.1.1 is the default).
    //-------------------------------------------------------------------------
    enum class Protocol
    {
        V5, V311, V31
    };

    //-------------------------------------------------------------------------
    //! \brief Will the broker preserve or clean client subscriptions and
    //! messages when the client disconnect.
    //-------------------------------------------------------------------------
    enum class Session
    {
        //! \brief Preserve all messages and subscriptions on disconnect.
        Preserve,
        //! \brief Clean all messages and subscriptions on disconnect.
        Cleanup
    };

    //-------------------------------------------------------------------------
    //! \brief Quality of service for message delivery.
    //-------------------------------------------------------------------------
    enum class QoS
    {
        QoS0, //! \brief At most once
        QoS1, //! \brief At least once
        QoS2  //! \brief Exactly once
    };

    //-------------------------------------------------------------------------
    //! \brief Status of the MQTT client.
    //-------------------------------------------------------------------------
    enum class Status
    {
        DISCONNECTED, //! \brief Not connected to the MQTT broker.
        CONNECTED,    //! \brief Connected to the MQTT broker.
        IN_DEFECT     //! \brief Fatal internal MQTT failure.
    };

    //-------------------------------------------------------------------------
    //! \brief
    //-------------------------------------------------------------------------
    struct Version
    {
        //! \brief Return the mosquitto C library version.
        int mosquitto[3] = {0, 0, 0};
        //! \brief Return the C++ wrapper version.
        int wrapper[3] = {0, 2, 0};
        //! \brief Protocol version.
        int protocol[3] = {0, 0, 0};
    };

    //-------------------------------------------------------------------------
    //! \brief Settings used for the creation of MQTT client.
    //-------------------------------------------------------------------------
    struct Settings
    {
        //! \brief The client ID that shall be unique for the MQTT broker.
        //! Let it dummy to let the MQTT broker generate a random name.
        std::string client_id;
        //! \brief MQTT protocol version.
        MQTT::Protocol protocol = MQTT::Protocol::V5;
        //! \brief Will the broker reserve or clean all client messages and
        //! subscriptions when the client disconnect.
        MQTT::Session clean_session = MQTT::Session::Cleanup;
    };

    //-------------------------------------------------------------------------
    //! \brief Settings used for the connection to the MQTT broker.
    //-------------------------------------------------------------------------
    struct Connection
    {
        //! \brief addr the string of the ip address (ie "localhost").
        std::string address = "localhost";
        //! \brief port the port of the MQTT broker (default is 1883).
        size_t port = 1883;
        //! \brief Connection timeout in seconds.
        std::chrono::seconds timeout = std::chrono::seconds(60);
    };

    //-------------------------------------------------------------------------
    //! \brief An MQTT topic is and identifier used by the broker to filter
    //! messages from connected clients.
    //-------------------------------------------------------------------------
    struct Topic
    {
        //! \brief Name of the topic.
        std::string name;
        //! \brief Retain ?
        bool retain = false;
        //! \brief Unique Id set during the subscription.
        int id = 0;
    };

    //-------------------------------------------------------------------------
    //! \brief C++ Wrapper on the C struct mosquitto_message with some helper
    //! methods. Note that message returned by the subscribe() callback or by
    //! the onMessageReceived() method, is temporary and will be freed by the
    //! library after the callback completes.
    //!
    //! Fields of the mosquitto_message are:
    //!  - uint16_t mid: the message id of the sent message.
    //!  - char *topic;
    //!  - uint8_t *payload;
    //!  - uint32_t payloadlen;
    //!  - int qos;
    //!  - bool retain;
    //-------------------------------------------------------------------------
    struct Message : mosquitto_message
    {
        //---------------------------------------------------------------------
        //! \brief Since received message payloads are temporary, when callback
        //! such as onMessageReceived() the payload content is no longer available
        //! once the callback is ended. A deep copy of the payload may be needed
        //! for later access.
        //! This helper copy the payload content into the given container. The
        //! buffer lenght is managed by this method.
        //! \param[inout] buffer where to store the payload.
        //! \param[in] clear if set to true the container has its content removed
        //! before payload is stored. Else, payload content is appeneded.
        //! \return the new container number of bytes.
        //---------------------------------------------------------------------
        size_t store(std::vector<uint8_t>& buffer, bool const clear = true)
        {
            if (clear) {
                buffer.clear();
            }
            buffer.resize(buffer.size() + this->payloadlen);
            std::memcpy(&buffer[buffer.size() - this->payloadlen], this->payload,
                this->payloadlen * sizeof(uint8_t));
            return buffer.size();
        }

        //---------------------------------------------------------------------
        //! \brief Cast the payload into the desired struct/class passed as
        //! template. Beware this returns a reference on a temporary memory that
        //! will be freed by the library after the callback completes. The client
        //! should make a copy of the class if he desires to keep it.
        //---------------------------------------------------------------------
        template<class T>
        T const& cast_to() const
        {
            assert((size_t(payloadlen) == sizeof(T)) && "incompatible size");
            return *reinterpret_cast<const T*>(payload);
        }
    };

    //-------------------------------------------------------------------------
    //! \brief Callback triggered when a new message is received.
    //-------------------------------------------------------------------------
    using ReceptionCallback = std::function<void(Message const& message)>;

    //-------------------------------------------------------------------------
    //! \brief Callback triggered when the MQTT client just connects to the
    //! MQTT broker.
    //-------------------------------------------------------------------------
    using ConnectionCallback = std::function<void(int rc)>;

    //-------------------------------------------------------------------------
    //! \brief
    //-------------------------------------------------------------------------
    MQTT(MQTT::Settings const& settings = {"", MQTT::Protocol::V5, MQTT::Session::Cleanup});

    //-------------------------------------------------------------------------
    //! \brief Release memory. Clean up of the mosquitto library if no other
    //! clients is using the mosquitto library.
    //-------------------------------------------------------------------------
    virtual ~MQTT();

    //-------------------------------------------------------------------------
    //! \brief Return
    //-------------------------------------------------------------------------
    MQTT::Version const& version() const { return m_version; }

    //-------------------------------------------------------------------------
    //! \brief Return the current client status.
    //-------------------------------------------------------------------------
    MQTT::Status status() const { return m_status; }

    //-------------------------------------------------------------------------
    //! \brief Return the last error.
    //-------------------------------------------------------------------------
    std::error_code const& error() const { return m_error; }

    //-------------------------------------------------------------------------
    //! \brief Return the last error.
    //-------------------------------------------------------------------------
    static std::string error(int const ec) { return mosquitto_strerror(ec); }

    //-------------------------------------------------------------------------
    //! \brief Non blocking connection to the MQTT broker as random client id.
    //! \return true if not internal error occured, else return false.
    //! \param[in] onConnected lambda function used as callback called when the
    //! client is connected to the MQTT broker. Set it to nullptr to force calling
    //! override method onConnected() instead.
    //! \param[in] onDisconnected lambda function used as callback called when
    //! the client has disconnected from the MQTT broker. Set it to nullptr to
    //! force calling override method onConnected() instead.
    //-------------------------------------------------------------------------
    bool connect(Connection const settings,
                 MQTT::ConnectionCallback onConnected = nullptr,
                 MQTT::ConnectionCallback onDisconnected = nullptr);

    //-------------------------------------------------------------------------
    //! \brief Disconnect from the broker. The callback onConnected() either
    //! the lambda or the method will be called.
    //-------------------------------------------------------------------------
    bool disconnect();

    //-------------------------------------------------------------------------
    //! \brief Subscription to given topic wiith quality of service and optional
    //! callback reacting to incoming message from the given topic.
    //! \note Be sure the client is connected to the broker before subscribing.
    //! Call this method for example inside the onConnected callback.
    //! \param[in] onMessageReceived lambda function used as callback called when
    //! a new message has been received from the MQTT broker. Set it to nullptr
    //! to force calling override method onConnected() instead.
    //! \param[in] topic the desired topic.
    //! \param[in] qos the desired quality of service.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool subscribe(MQTT::Topic& topic, QoS const qos,
                   MQTT::ReceptionCallback onMessageReceived = nullptr);

    //-------------------------------------------------------------------------
    //! \brief Remove the subscription of the given topic.
    //! \param[in] topic the desired topic to remove.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool unsubscribe(MQTT::Topic& topic);

    //-------------------------------------------------------------------------
    //! \brief Send a string message to the given topic with using a desired
    //! quality of service.
    //! \param[in] topic the desired topic to send this message on.
    //! \param[in] payload the message content as std::string to send.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool publish(MQTT::Topic& topic, std::string const& payload, QoS const qos);

    //-------------------------------------------------------------------------
    //! \brief Send a vector of bytes message to the given topic using a desired
    //! quality of service.
    //! \param[in] topic the desired topic to send this message on.
    //! \param[in] payload the message content as std::vector of char to send.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool publish(MQTT::Topic& topic, std::vector<uint8_t> const& payload, QoS const qos);

    //-------------------------------------------------------------------------
    //! \brief Send a raw array of bytes message to the given topic using a
    //! desired quality of service.
    //! \param[in] topic the desired topic to send this message on.
    //! \param[in] payload the message content as pointer of char array to send.
    //! \param[in] size the number of bytes of the payload.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool publish(MQTT::Topic& topic, uint8_t const* payload, size_t const size, QoS const qos);

protected:

    struct mosquitto* mosquitto() { return m_mosquitto; }

// Callbacks to override in the case of we do not use lambda functions for
// implementing callbacks.
private:

    //-------------------------------------------------------------------------
    //! \brief Callback called when the connection with the MQTT broker is made.
    //! \param[in] message the C struct of the mosquitto lib holding the received
    //! message.
    //! This variable and associated memory will be freed by the library after
    //! the callback completes. The client should make copies of any of the data
    //! it requires.
    //-------------------------------------------------------------------------
    virtual void onMessageReceived(MQTT::Message const& /*message*/) {}

    //-------------------------------------------------------------------------
    //! \brief Callback called when the connection with the MQTT broker is made.
    //! This is called when the broker sends a CONNACK message in response to a
    //! connection.
    //! \param[in] rc the mosquitto return code of the connection response.
    //-------------------------------------------------------------------------
    virtual void onConnected(int /*rc*/) {}

    //-------------------------------------------------------------------------
    //! \brief Callback called when a disconnection with the MQTT broker occured.
    //! This is called when the broker has received the DISCONNECT command and has
    //! disconnected the client.
    //! \param[in] rc the mosquitto return code of the connection response.
    //! A value of 0 means the client has called mosquitto_disconnect. Any other
    //! value indicates that the disconnect is unexpected.
    //-------------------------------------------------------------------------
    virtual void onDisconnected(int /*rc*/) {}

    //-------------------------------------------------------------------------
    //! \brief Callback called when a message has been sent to the broker
    //! successfully.
    //! \param[in] mid the message id of the sent message.
    //-------------------------------------------------------------------------
    virtual void onPublished(int /*mid*/) {}

    //-------------------------------------------------------------------------
    //! \brief Callback called when the broker responds to a subscription request.
    //! \param[in] mid the message id of the sent message.
    //! \param[in] qos_counnt the number of granted subscriptions (size of granted_qos).
    //! \param[in] granted_qos an array of integers indicating the granted QoS
    //! for each of the subscriptions.
    //-------------------------------------------------------------------------
    virtual void onSubscribed(int /*mid*/, int /*qos_count*/, const int */*granted_qos*/) {}

    //-------------------------------------------------------------------------
    //! \brief Callback called when the broker responds to a unsubscription request.
    //! \param[in] mid the message id of the sent message.
    //-------------------------------------------------------------------------
    virtual void onUnsubscribed(int /*mid*/) {}

private:

    bool instantiate(char const* client_id, const bool clean_session);

    static void on_message_received_wrapper(struct mosquitto*, void *, const struct mosquitto_message *);
    static void on_connected_wrapper(struct mosquitto*, void*, int);
    static void on_disconnected_wrapper(struct mosquitto*, void*, int);

    static void on_published_wrapper(struct mosquitto* /*mosqitto*/, void* userdata, int mid)
    {
        MQTT* mqtt = static_cast<MQTT*>(userdata);
        assert((mqtt != nullptr) && "null pointer passed as param");
        mqtt->onPublished(mid);
    }

    static void on_subscribed_wrapper(struct mosquitto* /*mosqitto*/, void *userdata, int mid, int qos_count, const int *granted_qos)
    {
        MQTT* mqtt = static_cast<MQTT*>(userdata);
        assert((mqtt != nullptr) && "null pointer passed as param");
        mqtt->onSubscribed(mid, qos_count, granted_qos);
    }

    static void on_unsubscribed_wrapper(struct mosquitto* /*mosqitto*/, void *userdata, int mid)
    {
        MQTT* mqtt = static_cast<MQTT*>(userdata);
        assert((mqtt != nullptr) && "null pointer passed as param");
        mqtt->onUnsubscribed(mid);
    }

    //! \brief Count the number of user using MQTT.
    //! This internal counter is needed for init the C library for the first user
    //! and clean it when the last user no longer used it.
    static size_t& libMosquittoCountInstances()
    {
        static size_t counter = 0u;
        return counter;
    }

    //! \brief To be called before using MQTT. Init the C library.
    bool libMosquittoInit(MQTT::Protocol protocol);

    //! \brief To be called when MQTT is no longer used. Clean the C library.
    void libMosquittoCleanUp();

private:

    //! \brief The instance of the mosquitto library.
    struct mosquitto *m_mosquitto = nullptr;
    //!
    Version m_version;

    struct {
        //! \brief Callbacks when a message has been received.
        std::map<std::string, MQTT::ReceptionCallback> reception;
        ConnectionCallback connection = nullptr;
        ConnectionCallback disconnection = nullptr;
    } m_callbacks;

    //! \brief Hold the last error.
    std::error_code m_error;
    //! \brief Hold the connection status.
    std::atomic<MQTT::Status> m_status{MQTT::Status::DISCONNECTED};
};

//-----------------------------------------------------------------------------
template<>
inline std::string const& MQTT::Message::cast_to<std::string>() const
{
    static std::string msg;
    
    msg = static_cast<const char*>(payload);
    return msg;
}

#endif