#include "MQTT/MQTT.hpp"
#include <iostream>
#include <thread>

// *****************************************************************************
//! \brief This inherited class is optional. Use inheritance is you do not want
//! to use callback and change the default behavior.
// *****************************************************************************
class InheritedMQTT: public MQTT
{
public:

    InheritedMQTT() : MQTT()
    {}

    InheritedMQTT(MQTT::Settings const& settings) : MQTT(settings)
    {}

private:

    //-------------------------------------------------------------------------
    //! \brief Callback when this class is connected to the MQTT broker.
    //-------------------------------------------------------------------------
    virtual void onConnected(int rc) override
    {
        std::cout << "[InheritedMQTT] Connected to MQTT broker with status '"
                    << MQTT::error(rc) << "': " << rc << std::endl;

        static MQTT::Topic INPUT_TOPIC{"Input", false, 0};

        // Callback reacting to message coming on the topic named "Input".
        // An echoed message is publish to the topic named "Output".
        if (!subscribe(INPUT_TOPIC, MQTT::QoS::QoS0))
        {
            std::cerr << "[InheritedMQTT] MQTT subscription failed: " 
                      << error().message() << std::endl;
        }
    }

    virtual void onMessageReceived(MQTT::Message const& msg) override
    {
        static MQTT::Topic OUTPUT_TOPIC{"Output", false, 0};

        // Conversion from raw C string into C++ string.
        std::string topic(msg.topic);
        std::string const& message = msg.cast_to<std::string>();

        // Example of usage of the fields.
        std::cout << "[InheritedMQTT] Received message " << msg.mid
                << ": \"" << message << "\""
                << " from topic: '" << topic << "'"
                << " size: " << msg.payloadlen
                << " qos: " << msg.qos
                << std::endl;

        // Send back the echoed message.
        publish(OUTPUT_TOPIC, message + " back from InheritedMQTT", MQTT::QoS::QoS0);
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when a disconnection with the MQTT broker occured.
    //! This is called when the broker has received the DISCONNECT command and has
    //! disconnected the client.
    //-------------------------------------------------------------------------
    virtual void onDisconnected(int rc) override
    {
        std::cout << "[InheritedMQTT] Disconnected. Reason was '"
                  << MQTT::error(rc) << "' (" << rc << ")" << std::endl;
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when a message has been sent to the broker
    //! successfully.
    //-------------------------------------------------------------------------
    virtual void onPublished(int mid) override
    {
        std::cout << "[InheritedMQTT] Message " << mid << " published" << std::endl;
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when the broker responds to a subscription request.
    //-------------------------------------------------------------------------
    virtual void onSubscribed(int mid, int /*qos_count*/, const int* /*granted_qos*/) override
    {
        std::cout << "[InheritedMQTT] Topic " << mid << " subscribed!" << std::endl;
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when the broker responds to a unsubscription request.
    //-------------------------------------------------------------------------
    virtual void onUnsubscribed(int mid) override
    {
        std::cout << "[InheritedMQTT] Message " << mid << " no longer subscribed!"
                  << std::endl;
    }
};

// *****************************************************************************
//! \brief A MQTT client using lambdas functions.
//! The client is connecting to the broker (address: localhost, port: 1883).
//! Once conncected, it will subscribe on the topic "Example1/Input" and all
//! received messages will be published back on the topic "Example1/Output".
// *****************************************************************************
class InheritanceClient
{
public:

    InheritanceClient()
        : m_mqtt({"example_inheritance_client", MQTT::Protocol::V5, MQTT::Session::Cleanup})
    {
        std::cout << "[InheritedMQTT] Versionning:" << std::endl;
        std::cout << "  Mosquitto: "
                  << m_mqtt.version().mosquitto[0] << "."
                  << m_mqtt.version().mosquitto[1] << "."
                  << m_mqtt.version().mosquitto[2] << std::endl;
        std::cout << "  C++ wrapper: "
                  << m_mqtt.version().wrapper[0] << "."
                  << m_mqtt.version().wrapper[1] << "."
                  << m_mqtt.version().wrapper[2] << std::endl;
        std::cout << "  MQTT protocol: "
                  << m_mqtt.version().protocol[0] << "."
                  << m_mqtt.version().protocol[1] << "."
                  << m_mqtt.version().protocol[2] << std::endl;

        if (!m_mqtt.connect({"localhost", 1883, std::chrono::seconds(60)}))
        {
            std::cerr << m_mqtt.error().message() << std::endl;
        }
        else
        {
            static MQTT::Topic OUTPUT_TOPIC{"Output", false, 0};
            m_mqtt.publish(OUTPUT_TOPIC, std::string("Hello from InheritedMQTT"), MQTT::QoS::QoS0);
        }
    }

private:

    InheritedMQTT m_mqtt;
};

// *****************************************************************************
//! \brief A MQTT client using lambdas functions.
//! The client is connecting to the broker (address: localhost, port: 1883).
//! Once conncected, it will subscribe on the topic "Example1/Input" and all
//! received messages will be published back on the topic "Example1/Output".
// *****************************************************************************
class LambdaClient
{
public:

    LambdaClient()
        : m_mqtt({"example_lambda_client", MQTT::Protocol::V5, MQTT::Session::Cleanup})
    {
        static MQTT::Topic INPUT_TOPIC{"Input", false, 0};
        static MQTT::Topic OUTPUT_TOPIC{"Output", false, 0};

        // --------------------------------------------------------------------
        static auto onMessageReceived = [&](const MQTT::Message& msg)
        {
            // Conversion from raw C string into C++ string.
            std::string topic(msg.topic);
            std::string const& message = msg.cast_to<std::string>();

            // Example of usage of the fields.
            std::cout << "[LambdaClient] Received message " << msg.mid
                    << ": \"" << message << "\""
                    << " from topic: '" << topic << "'"
                    << " size: " << msg.payloadlen
                    << " qos: " << msg.qos
                    << std::endl;

            // Send back the echoed message.
            m_mqtt.publish(OUTPUT_TOPIC, message + " back from LambdaClient", MQTT::QoS::QoS0);
        };

        // --------------------------------------------------------------------
        static auto onConnected = [&](int rc)
        {
            std::cout << "[LambdaClient] Connected to MQTT broker with status '"
                      << MQTT::error(rc) << "': " << rc << std::endl;

            // Callback reacting to message coming on the topic named "Input".
            // An echoed message is publish to the topic named "Output".
            if (!m_mqtt.subscribe(INPUT_TOPIC, MQTT::QoS::QoS0, onMessageReceived))
            {
                std::cerr << "[LambdaClient] MQTT subscription failed: "
                          << m_mqtt.error().message() << std::endl;
            }
        };

        // --------------------------------------------------------------------
        std::cout << "[LambdaClient] Versionning:" << std::endl;
        std::cout << "  Mosquitto: "
                  << m_mqtt.version().mosquitto[0] << "."
                  << m_mqtt.version().mosquitto[1] << "."
                  << m_mqtt.version().mosquitto[2] << std::endl;
        std::cout << "  C++ wrapper: "
                  << m_mqtt.version().wrapper[0] << "."
                  << m_mqtt.version().wrapper[1] << "."
                  << m_mqtt.version().wrapper[2] << std::endl;
        std::cout << "  MQTT protocol: "
                  << m_mqtt.version().protocol[0] << "."
                  << m_mqtt.version().protocol[1] << "."
                  << m_mqtt.version().protocol[2] << std::endl;

        // --------------------------------------------------------------------
        // Non blocking connection the MQTT broker. Once the client is connected to
        // the MQTT broker the lambda callback onConnected will be called and we will
        // subscribe to MQTT topics.
        if (!m_mqtt.connect({"localhost", 1883, std::chrono::seconds(60)}, onConnected))
        {
            std::cerr << m_mqtt.error().message() << std::endl;
        }
        else
        {
            m_mqtt.publish(OUTPUT_TOPIC, std::string("Hello from LambdaClient"), MQTT::QoS::QoS0);
        }
    }

private:

    MQTT m_mqtt;
};

// *****************************************************************************
//! \brief g++ --std=c++14 -Wall -Wextra -I../include ../src/MQTT.cpp Example.cpp
//! -o example `pkg-config --cflags --libs libmosquitto`
// *****************************************************************************
int main()
{
    LambdaClient client1;
    InheritanceClient client2;

    // Client is asynchronous.
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}