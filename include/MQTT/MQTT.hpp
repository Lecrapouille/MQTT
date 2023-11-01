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
#  include <cassert>

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
    //! \brief Quality of service for message delivery.
    //-------------------------------------------------------------------------
    enum class QoS {
        QoS0, //! \brief At most once
        QoS1, //! \brief At least once
        QoS2  //! \brief Exactly once
    };

    //-------------------------------------------------------------------------
    //! \brief Just a wrapper on the C struct mosquitto_message with some helper
    //! method. Note that message returned by the callback onMessageReceived is
    //! temporary and will be freed by the library after the callback completes.
    //! mosquitto_message Fields are:
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
        //! \brief Helper opy the payload content into the given container.
        //! \param[inout] container where to store the payload.
        //! \param[in] clear if set to true the container has its content removed
        //! before payload is stored. Else, payload content is appeneded.
        //! \return the new container number of bytes.
        //! \fixme NOT YET TESTED!
        //---------------------------------------------------------------------
        size_t store(std::vector<uint8_t>& container, bool const clear = true);

        //---------------------------------------------------------------------
        //! \brief Cast the payload into the desired struct/class passed as
        //! template. Beware this returns a reference on a temporary memory that
        //! will be freed by the library after the callback completes. The client
        //! should make a copy of the class if he desires to keep it.
        //! \fixme NOT YET TESTED!
        //---------------------------------------------------------------------
        template<class T>
        T& to()
        {
            assert((payloadlen == sizeof (T)) && "incompatible size");
            return *reinterpret_cast<T*>(payload);
        }
    };

    //-------------------------------------------------------------------------
    //! \brief Dummy constructor.
    //! No internal calls made execept calling the init of the mosquitto library.
    //-------------------------------------------------------------------------
    MQTT();

    //-------------------------------------------------------------------------
    //! \brief Release memory.
    //! No internal calls made execept calling the clean up of the mosquitto
    //! library.
    //-------------------------------------------------------------------------
    virtual ~MQTT();

    //-------------------------------------------------------------------------
    //! \brief Return the last error.
    //-------------------------------------------------------------------------
    std::string const& error() const { return m_error; }

    //-------------------------------------------------------------------------
    //! \brief Non blocking connection to the MQTT broker as random client id.
    //! \param[in] addr the string of the ip address (ie "localhost").
    //! \param[in] port the port of the MQTT broker (default is 1883).
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool connect(std::string const& addr, size_t const port = 1883);

    //-------------------------------------------------------------------------
    //! \brief Non blocking connection to the MQTT broker withe a client id.
    //! \param[in] id the client ID that shall be unique for the MQTT broker.
    //! \param[in] addr the string of the ip address (ie "localhost").
    //! \param[in] port the port of the MQTT broker (default is 1883).
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool connect(std::string const& id, std::string const& addr, size_t const port = 1883);

    //-------------------------------------------------------------------------
    //! \brief Subscription to given topic and quality of service.
    //! \param[in] topic the desired topic.
    //! \param[in] qos the dsired quality of service.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool subscribe(std::string const& topic, QoS const qos);

    //-------------------------------------------------------------------------
    //! \brief Remove the subscription of the given topic.
    //! \param[in] topic the desired topic to remove.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool unsubscribe(std::string const& topic);

    //-------------------------------------------------------------------------
    //! \brief Send a string message to the given topic with using a desired
    //! quality of service.
    //! \param[in] topic the desired topic to send this message on.
    //! \param[in] payload the message content as std::string to send.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool publish(std::string const& topic, std::string const& payload, QoS const qos);

    //-------------------------------------------------------------------------
    //! \brief Send a vector of bytes message to the given topic using a desired
    //! quality of service.
    //! \param[in] topic the desired topic to send this message on.
    //! \param[in] payload the message content as std::vector of char to send.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool publish(std::string const& topic, std::vector<uint8_t> const& payload, QoS const qos);

    //-------------------------------------------------------------------------
    //! \brief Send a raw array of bytes message to the given topic using a
    //! desired quality of service.
    //! \param[in] topic the desired topic to send this message on.
    //! \param[in] payload the message content as pointer of char array to send.
    //! \param[in] size the number of bytes of the payload.
    //! \return true if not internal error occured, else return false.
    //-------------------------------------------------------------------------
    bool publish(std::string const& topic, const uint8_t* payload, size_t const size, QoS const qos);

protected:

    struct mosquitto* mosquitto() { return m_mosquitto; }

private: // Callbacks

    //-------------------------------------------------------------------------
    //! \brief Callback called when the connection with the MQTT broker is made.
    //! This is called when the broker sends a CONNACK message in response to a
    //! connection.
    //! \param[in] rc the mosquitto return code of the connection response.
    //-------------------------------------------------------------------------
    virtual void onConnected(int /*rc*/) = 0;

    //-------------------------------------------------------------------------
    //! \brief Callback called when the connection with the MQTT broker is made.
    //! \param[in] message the C struct of the mosquitto lib holding the received
    //! message.
    //! This variable and associated memory will be freed by the library after
    //! the callback completes.  The client should make copies of any of the data
    //! it requires.
    //-------------------------------------------------------------------------
    virtual void onMessageReceived(const MQTT::Message& message) = 0;

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

    bool doConnection(const char* client_id, const char* addr, size_t const port);

    static void on_connected_wrapper(struct mosquitto* /*mosqitto*/, void* userdata, int rc)
    {
        MQTT* mqtt = static_cast<MQTT*>(userdata);
        assert((mqtt != nullptr) && "null pointer passed as param");
        mqtt->onConnected(rc);
    }

    static void on_disconnected_wrapper(struct mosquitto* /*mosqitto*/, void* userdata, int rc)
    {
        MQTT* mqtt = static_cast<MQTT*>(userdata);
        assert((mqtt != nullptr) && "null pointer passed as param");
        mqtt->onDisconnected(rc);
    }

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

    static void on_message_received_wrapper(struct mosquitto* /*mosqitto*/, void *userdata, const struct mosquitto_message *message)
    {
        MQTT* mqtt = static_cast<MQTT*>(userdata);
        assert((mqtt != nullptr) && "null pointer passed as param");
        mqtt->onMessageReceived(*reinterpret_cast<const MQTT::Message*>(message));
    }

    void libMosquittoInit();
    void libMosquittoCleanUp();
    static size_t& libMosquittoCountInstances()
    {
        static size_t counter = 0u;
        return counter;
    }

private:

    //! \brief The instance of the mosquitto library.
    struct mosquitto *m_mosquitto = nullptr;
    //! \brief Hold the last error.
    std::string m_error;
};

#endif
