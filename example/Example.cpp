#include "MQTT/MQTT.hpp"
#include <iostream>
#include <thread>

// *****************************************************************************
//! \brief This inherited class is optional. Use inheritance is you do not want
//! to use callback and change the default behavior.
// *****************************************************************************
class ClientMQTT: public MQTT
{
public:
    ClientMQTT() : MQTT()
    {}

    ClientMQTT(MQTT::Settings const& settings) : MQTT(settings)
    {}

private:
    //-------------------------------------------------------------------------
    //! \brief Callback when this class is connected to the MQTT broker.
    //-------------------------------------------------------------------------
    virtual void onConnected(int rc) override
    {
        std::cout << "Connected to MQTT broker with return code " << rc << std::endl;
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when a disconnection with the MQTT broker occured.
    //! This is called when the broker has received the DISCONNECT command and has
    //! disconnected the client.
    //-------------------------------------------------------------------------
    virtual void onDisconnected(int rc) override
    {
        std::cout << "Disconnected with return code: " << rc << std::endl;
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when a message has been sent to the broker
    //! successfully.
    //-------------------------------------------------------------------------
    virtual void onPublished(int mid) override
    {
        std::cout << "Message " << mid << " published" << std::endl;
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when the broker responds to a subscription request.
    //-------------------------------------------------------------------------
    virtual void onSubscribed(int mid, int /*qos_count*/, const int* /*granted_qos*/) override
    {
        std::cout << "Topic " << mid << " subscribed!" << std::endl;
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when the broker responds to a unsubscription request.
    //-------------------------------------------------------------------------
    virtual void onUnsubscribed(int mid) override
    {
        std::cout << "Message " << mid << " no longer subscribed!" << std::endl;
    }
};

// *****************************************************************************
//! \brief A MQTT client using lambdas functions.
//! The client is connecting to the broker (address: localhost, port: 1883).
//! Once conncected, it will subscribe on the topic "Example1/Input" and all
//! received messages will be published back on the topic "Example1/Output".
// *****************************************************************************
class Client
{
public:

    Client() : m_mqtt({"example_client_id", MQTT::Settings::CLEAN_SESSION})
    {
        static MQTT::Topic INPUT_TOPIC{"Input"};
        static MQTT::Topic OUTPUT_TOPIC{"Output"};

        // --------------------------------------------------------------------
        auto onMessageReceived = [&](const MQTT::Message& msg)
        {
            // Conversion from raw C string into C++ string.
            std::string topic(msg.topic);
            std::string const& message = msg.cast_to<std::string>();

            // Example of usage of the fields.
            std::cout << "Received message " << msg.mid
                    << ": \"" << message << "\""
                    << " from topic: '" << topic << "'"
                    << " size: " << msg.payloadlen
                    << " qos: " << msg.qos
                    << std::endl;

            // Send back the echoed message.
            m_mqtt.publish(OUTPUT_TOPIC, message + " back", MQTT::QoS::QoS0);
        };

        // --------------------------------------------------------------------
        auto onConnected = [&](int rc)
        {
            std::cout << "Connected to MQTT broker with return code " << rc << std::endl;

            // Callback reacting to message coming on the topic named "Input".
            // An echoed message is publish to the topic named "Output".
            if (!m_mqtt.subscribe(INPUT_TOPIC, MQTT::QoS::QoS0, onMessageReceived))
            {
                std::cerr << "MQTT subscription failed: " << m_mqtt.error().message() << std::endl;
            }
        };

        // --------------------------------------------------------------------
        // Non blocking connection the MQTT broker. Once the client is connected to
        // the MQTT broker the lambda callback onConnected will be called and we will
        // subscribe to MQTT topics.
        if (!m_mqtt.connect({"localhost", 1883, std::chrono::seconds(60)}, onConnected))
        {
            std::cerr << m_mqtt.error().message() << std::endl;
        }
    }

private:

    ClientMQTT m_mqtt;
};

// *****************************************************************************
//! \brief g++ --std=c++14 -Wall -Wextra -I../include ../src/MQTT.cpp Example.cpp
//! -o example `pkg-config --cflags --libs libmosquitto`
// *****************************************************************************
int main()
{
    Client client;

    // Client is asynchronous.
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}