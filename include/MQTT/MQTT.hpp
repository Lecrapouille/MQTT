//*****************************************************************************
// A C++ class wrapping Mosquitto MQTT https://github.com/eclipse/mosquitto
//
// MIT License
//
// Copyright (c) 2024 Quentin Quadrat <lecrapouille@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//*****************************************************************************

#ifndef ASYNC_MQTT_CLIENT_HPP
#  define ASYNC_MQTT_CLIENT_HPP

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

namespace mqtt {

// ****************************************************************************
//! \brief MQTT Protocol version managed (3.1.1 is the default).
// ****************************************************************************
enum class Protocol
{
    V31 = MQTT_PROTOCOL_V31,
    V311 = MQTT_PROTOCOL_V311,
    V5 = MQTT_PROTOCOL_V5
};

// ****************************************************************************
//! \brief Quality of service for message delivery.
// ****************************************************************************
enum class QoS
{
    QoS0, //! \brief At most once
    QoS1, //! \brief At least once
    QoS2  //! \brief Exactly once
};

// ****************************************************************************
//! \brief An MQTT topic is and identifier used by the broker to filter
//! messages from connected clients.
// ****************************************************************************
struct Topic
{
    //! \brief Name of the topic.
    std::string name;
    //! \brief Retain ?
    bool retain = false;
    //! \brief Unique Id set during the subscription.
    int id = 0;
};

// ****************************************************************************
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
// ****************************************************************************
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

//-----------------------------------------------------------------------------
template<>
inline std::string const& Message::cast_to<std::string>() const
{
    static std::string msg;

    msg = static_cast<const char*>(payload);
    return msg;
}

// ****************************************************************************
//! \brief Base class, not thread safe, offering an asynchronous MQTT client
//! based on the mosquitto C lib implementing MQTT v3.1.1, v5 protocols. The
//! MQTT protocol is based on publishing and subscribing messages on topics (aka
//! channels). A server, called broker allows to dispatch messages to clients
//! and manage quality of services (different strategies to lost messages).
//!
//! The server part is already managed by mosquitto lib once installed on your
//! system. As consequence, this API only offers client side class.
//!
//! You have two strategies for implementing reactions for your client: the
//! first is based on lambda functions triggered on events such as connection,
//! deconnection, message received, subscription. The second strategy is based
//! on overriden virtual private methods currently dummy.
//!
//! For the connection, if you choose the first strategy, you have to pass a
//! lambda function onConnected as callback to the method connect(). If you
//! choose the second strategy, you have to pass nullptr as callback to the
//! method connect() but you have to override the method onConnected().
//!
//! The easy part is for publishing messages: once your client is connected to
//! the broker, you call one of the publish() methods, dependig on the type of
//! your payload. No callback is needed.
//!
//! For receiving messages. If you choose the first strategy, you have to pass a
//! lambda function onMessageReceived as callback to the method
//! subscribe(). Beware the subscribe() shall be called once the client is
//! connected to the broker. To be sure to not call it before connection, manage
//! subscriptions inside the onConnected callback.
//!
//! For receiving messages. If you choose the second strategy, override the
//! onMessageReceived() method. Contrary to first strategy, you have to filter
//! by yourself messages by their topic.
//!
//! Mostly of methods of this API return true in case of success or return false
//! in case of failure and for getting the reason of the failure, you shall call
//! error() (to get the human readable message error().message).
//!
//! Since this class wraps the mosquitto lib, you can access to C fucntions
//! thanks to the getter mosquitto(). See
//! https://mosquitto.org/api/files/mosquitto-h.html
//!
//! References:
//!   - https://github.com/eclipse/mosquitto
//!   - https://www.howtoforge.com/how-to-install-mosquitto-mqtt-message-broker-on-debian-11/
// ****************************************************************************
class Client
{
public:

    //-------------------------------------------------------------------------
    //! \brief Status of the MQTT client.
    //-------------------------------------------------------------------------
    enum class Status
    {
        Disconnected, //! \brief Not connected to the MQTT broker.
        Connected,    //! \brief Connected to the MQTT broker.
        InDefect      //! \brief Fatal internal MQTT failure.
    };


    //-------------------------------------------------------------------------
    //! \brief Will the broker preserve or clean client subscriptions and
    //! messages when the client disconnect?
    //-------------------------------------------------------------------------
    enum class Session
    {
        //! \brief Preserve all messages and subscriptions on disconnect.
        Preserve,
        //! \brief Clean all messages and subscriptions on disconnect.
        Cleanup
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
        Protocol protocol = Protocol::V5;
        //! \brief Will the broker reserve or clean all client messages and
        //! subscriptions when the client disconnect.
        Session session = Session::Cleanup;
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
    //! \brief Version of the C.
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
    //! \brief Callback triggered when a new message is received.
    //-------------------------------------------------------------------------
    using ReceptionCallback = std::function<void(Message const& message)>;

    //-------------------------------------------------------------------------
    //! \brief Callback triggered when the MQTT client just connects to the
    //! MQTT broker.
    //-------------------------------------------------------------------------
    using ConnectionCallback = std::function<void(int rc)>;

    //-------------------------------------------------------------------------
    //! \brief Initialize the mosquitto library and create a handle of the C
    //! lib.
    //! \param[in] settings configure the MQTT client: client id, MQTT protocol
    //! version, cleanup or preserver message on deconnection.
    //-------------------------------------------------------------------------
    Client(Client::Settings const& settings = {"", Protocol::V5, Session::Cleanup});

    //-------------------------------------------------------------------------
    //! \brief Release memory. Clean up of the mosquitto library if no other
    //! clients is using the mosquitto library.
    //-------------------------------------------------------------------------
    virtual ~Client();

    //-------------------------------------------------------------------------
    //! \brief Return version of the MQTT protocol, moquito lib and this API.
    //-------------------------------------------------------------------------
    Client::Version const& version() const { return m_version; }

    //-------------------------------------------------------------------------
    //! \brief Return the current client status.
    //-------------------------------------------------------------------------
    Client::Status status() const { return m_status; }

    //-------------------------------------------------------------------------
    //! \brief Return the last error. To be used when a method of the API has
    //! returned false.
    //-------------------------------------------------------------------------
    std::error_code const& error() const { return m_error; }

    //-------------------------------------------------------------------------
    //! \brief Return human readable message depending on the error code.
    //-------------------------------------------------------------------------
    static std::string error(int const ec) { return mosquitto_strerror(ec); }

    //-------------------------------------------------------------------------
    //! \brief Non blocking connection to the MQTT broker as random client id.
    //! \return true if not internal error occured, else return false.
    //! \param[in] onConnected lambda function used as callback called when the
    //! client is connected to the MQTT broker. Set it to nullptr to force
    //! calling override method onConnected() instead.
    //! \param[in] onDisconnected lambda function used as callback called when
    //! the client has disconnected from the MQTT broker. Set it to nullptr to
    //! force calling override method onConnected() instead.
    //-------------------------------------------------------------------------
    bool connect(Connection const settings,
                 Client::ConnectionCallback onConnected = nullptr,
                 Client::ConnectionCallback onDisconnected = nullptr);

    //-------------------------------------------------------------------------
    //! \brief Disconnect from the broker. The callback onConnected() or the
    //! lambda will be called depending on parameter passed to connect().
    //-------------------------------------------------------------------------
    bool disconnect();

    //-------------------------------------------------------------------------
    //! \brief Subscription to given topic with quality of service and optional
    //! callback reacting to incoming message for the given topic.
    //! \note Be sure the client is connected to the broker before subscribing.
    //! Call this method for example inside the onConnected callback.
    //! \param[in] onMessageReceived lambda function used as callback called when
    //! a new message has been received from the MQTT broker. Set it to nullptr
    //! to force calling override method onConnected() instead.
    //! \param[in] topic the desired topic.
    //! \param[in] qos the desired quality of service.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool subscribe(Topic& topic, QoS const qos,
                   Client::ReceptionCallback onMessageReceived = nullptr);

    //-------------------------------------------------------------------------
    //! \brief Remove the subscription of the given topic.
    //! \param[in] topic the desired topic to remove.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool unsubscribe(Topic& topic);

    //-------------------------------------------------------------------------
    //! \brief Send a string message to the given topic with using a desired
    //! quality of service.
    //! \param[in] topic the desired topic to send this message on.
    //! \param[in] payload the message content as std::string to send.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool publish(Topic& topic, std::string const& payload, QoS const qos);

    //-------------------------------------------------------------------------
    //! \brief Send a vector of bytes message to the given topic using a desired
    //! quality of service.
    //! \param[in] topic the desired topic to send this message on.
    //! \param[in] payload the message content as std::vector of char to send.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool publish(Topic& topic, std::vector<uint8_t> const& payload, QoS const qos);

    //-------------------------------------------------------------------------
    //! \brief Send a raw array of bytes message to the given topic using a
    //! desired quality of service.
    //! \param[in] topic the desired topic to send this message on.
    //! \param[in] payload the message content as pointer of char array to send.
    //! \param[in] size the number of bytes of the payload.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool publish(Topic& topic, uint8_t const* payload, size_t const size, QoS const qos);

protected:

    struct mosquitto* mosquitto() { return m_mosquitto; }

// Callbacks to override in the case of we do not use lambda functions for
// implementing callbacks.
private:

    //-------------------------------------------------------------------------
    //! \brief Callback called when the connection with the MQTT broker is made.
    //! \param[in] message the C struct of the mosquitto lib holding the
    //! received message.
    //! This variable and associated memory will be freed by the library after
    //! the callback completes. The client should make copies of any of the data
    //! it requires.
    //-------------------------------------------------------------------------
    virtual void onMessageReceived(Message const& /*message*/) {}

    //-------------------------------------------------------------------------
    //! \brief Callback called when the connection with the MQTT broker is made.
    //! This is called when the broker sends a CONNACK message in response to a
    //! connection.
    //! \param[in] rc the mosquitto return code of the connection response.
    //-------------------------------------------------------------------------
    virtual void onConnected(int /*rc*/) {}

    //-------------------------------------------------------------------------
    //! \brief Callback called when a disconnection with the MQTT broker
    //! occured.  This is called when the broker has received the DISCONNECT
    //! command and has disconnected the client.
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
    //! \brief Callback called when the broker responds to a subscription
    //! request.
    //! \param[in] mid the message id of the sent message.
    //! \param[in] qos_counnt the number of granted subscriptions (size of
    //! granted_qos).
    //! \param[in] granted_qos an array of integers indicating the granted QoS
    //! for each of the subscriptions.
    //-------------------------------------------------------------------------
    virtual void onSubscribed(int /*mid*/, int /*qos_count*/, const int */*granted_qos*/) {}

    //-------------------------------------------------------------------------
    //! \brief Callback called when the broker responds to a unsubscription
    //! request.
    //! \param[in] mid the message id of the sent message.
    //-------------------------------------------------------------------------
    virtual void onUnsubscribed(int /*mid*/) {}

private:

    bool instantiate(char const* client_id, const bool clean_session);

    static void on_message_received_wrapper(
        struct mosquitto*, void *, const struct mosquitto_message *);
    static void on_connected_wrapper(struct mosquitto*, void*, int);
    static void on_disconnected_wrapper(struct mosquitto*, void*, int);

    static void on_published_wrapper(struct mosquitto*, void* userdata, int mid)
    {
        Client* client = static_cast<Client*>(userdata);
        assert((client != nullptr) && "null pointer passed as param");
        client->onPublished(mid);
    }

    static void on_subscribed_wrapper(struct mosquitto*, void *userdata, int mid,
        int qos_count, const int *granted_qos)
    {
        Client* client = static_cast<Client*>(userdata);
        assert((client != nullptr) && "null pointer passed as param");
        client->onSubscribed(mid, qos_count, granted_qos);
    }

    static void on_unsubscribed_wrapper(struct mosquitto*, void *userdata, int mid)
    {
        Client* client = static_cast<Client*>(userdata);
        assert((client != nullptr) && "null pointer passed as param");
        client->onUnsubscribed(mid);
    }

    //-------------------------------------------------------------------------
    //! \brief Return the reference to the number of users using MQTT.
    //! This internal counter is needed for init the C library for the first
    //! user and clean it when the last user no longer used it.
    //-------------------------------------------------------------------------
    static size_t& libMosquittoCountInstances()
    {
        static size_t counter = 0u;
        return counter;
    }

    //-------------------------------------------------------------------------
    //! \brief To be called before using MQTT. Init the C library.
    //! \return true in case of sucess.
    //-------------------------------------------------------------------------
    bool libMosquittoInit(Protocol protocol);

    //-------------------------------------------------------------------------
    //! \brief To be called when MQTT is no longer used. Clean the C library.
    //-------------------------------------------------------------------------
    void libMosquittoCleanUp();

private:

    //! \brief The instance of the mosquitto library.
    struct mosquitto *m_mosquitto = nullptr;
    //! \brief MQTT protocol version, mosquito version, this APi version.
    Version m_version;
    //! \brief Store reactions
    struct {
        //! \brief Callback when a message has been received on the given
        //! topic.
        std::map<std::string, Client::ReceptionCallback> reception;
        //! \brief Callback when the client has been connected to the broker.
        ConnectionCallback connection = nullptr;
        //! \brief Callback when the client has been disconnected from the
        //! broker.
        ConnectionCallback disconnection = nullptr;
    } m_callbacks;
    //! \brief Hold the last error.
    std::error_code m_error;
    //! \brief Hold the connection status.
    std::atomic<Client::Status> m_status{Client::Status::Disconnected};
};

} // namespace mqtt

#endif // ASYNC_MQTT_CLIENT_HPP