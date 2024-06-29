#include "MQTT/MQTT.hpp"

#include <chrono>
#include <thread>
#include <iostream>
#include <atomic>

// *****************************************************************************
//! \brief A MQTT client connecting to the broker (localhost port 1883). Once
//! conncected it will subscribe to the topic "Example/Input" and all messages
//! it will receive will be published to the topic "Example/Output".
// *****************************************************************************
class MQTTExample: public MQTT
{
public:

    //-------------------------------------------------------------------------
    //! \brief Default constructor with non blocking connection to the MQTT
    //! broker.
    //-------------------------------------------------------------------------
    MQTTExample() : MQTT(/* "my client id", */"localhost", 1883)
    {}

private: // override MQTT callbacks

    //-------------------------------------------------------------------------
    //! \brief Callback when this class is connected to the MQTT broker.
    //-------------------------------------------------------------------------
    virtual void onConnected(int rc) override
    {
        std::cout << "Connected to MQTT broker with error code " << rc << std::endl;

        subscribe("Input", [&](const MQTT::Message& msg){
            std::string topic(static_cast<const char*>(msg.topic));

            // We exploit the fact that mosquitto appends an extra 0 bytes
            // that makes a direct reading as a C++ string.
            std::string message(static_cast<const char*>(msg.payload));
            // Alternative:
            // std::string const& message = msg.to<std::string>();
            std::cout << "Received message " << msg.mid
                    << ": \"" << message << "\""
                    << " from topic: '" << topic << "'"
                    << " size: " << msg.payloadlen
                    << " qos: " << msg.qos
                    << std::endl;
            // Send back the message.
            publish("Output", message + " back", MQTT::QoS::QoS0);
        }, MQTT::QoS::QoS0);
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when a disconnection with the MQTT broker occured.
    //! This is called when the broker has received the DISCONNECT command and has
    //! disconnected the client.
    //-------------------------------------------------------------------------
    virtual void onDisconnected(int rc)
    {
        std::cout << "Disconnected with error code: " << rc << std::endl;
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when a message has been sent to the broker
    //! successfully.
    //-------------------------------------------------------------------------
    virtual void onPublished(int mid)
    {
        std::cout << "Message " << mid << " published" << std::endl;
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when the broker responds to a subscription request.
    //-------------------------------------------------------------------------
    virtual void onSubscribed(int mid, int qos_count, const int *granted_qos)
    {
        std::cout << "Topic " << mid << " subscribed. Granted QOS:";
        for (int i = 0; i < qos_count; ++i)
        {
            std::cout << " " << granted_qos[i];
        }
        std::cout << std::endl;
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when the broker responds to a unsubscription request.
    //-------------------------------------------------------------------------
    virtual void onUnsubscribed(int mid)
    {
        std::cout << "Message " << mid << " no longer subscribed !" << std::endl;
    }
};

// *****************************************************************************
//! \brief g++ --std=c++11 -Wall -Wextra -I../include ../src/MQTT.cpp Example.cpp
//! -o example `pkg-config --cflags --libs libmosquitto`
// *****************************************************************************
int main()
{
    MQTTExample client; // Client is asynchronous.

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
