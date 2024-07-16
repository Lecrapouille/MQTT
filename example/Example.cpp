#include "MQTT/MQTT.hpp"
#include <iostream>
#include <thread>

using namespace mqtt;

// *****************************************************************************
//! \brief This inherited class is optional. Use inheritance is you do not want
//! to use callback and change the default behavior.
// *****************************************************************************
class InheritanceClient: public Client
{
public:

    InheritanceClient(Client::Settings const& settings)
        : Client(settings)
    {
        std::cout << "[InheritanceClient] Versionning:" << std::endl;
        std::cout << "  Mosquitto: "
                  << version().mosquitto[0] << "."
                  << version().mosquitto[1] << "."
                  << version().mosquitto[2] << std::endl;
        std::cout << "  C++ wrapper: "
                  << version().wrapper[0] << "."
                  << version().wrapper[1] << "."
                  << version().wrapper[2] << std::endl;
        std::cout << "  MQTT protocol: "
                  << version().protocol[0] << "."
                  << version().protocol[1] << "."
                  << version().protocol[2] << std::endl;

        if (!connect({"localhost", 1883, std::chrono::seconds(60)}))
        {
            std::cerr << error().message() << std::endl;
        }
        else
        {
            publish(OUTPUT_TOPIC, std::string("Hello from InheritanceClient"), QoS::QoS0);
        }
    }

private:

    //-------------------------------------------------------------------------
    //! \brief Callback when this class is connected to the MQTT broker.
    //-------------------------------------------------------------------------
    virtual void onConnected(int rc) override
    {
        std::cout << "[InheritanceClient] Connected to MQTT broker with status '"
                    << Client::error(rc) << "': " << rc << std::endl;

        // Callback reacting to message coming on the topic named "Input".
        // An echoed message is publish to the topic named "Output".
        if (!subscribe(INPUT_TOPIC, QoS::QoS0))
        {
            std::cerr << "[InheritanceClient] MQTT subscription failed: " 
                      << error().message() << std::endl;
        }
    }

    virtual void onMessageReceived(Message const& msg) override
    {
        // Conversion from raw C string into C++ string.
        std::string topic(msg.topic);
        std::string const& message = msg.cast_to<std::string>();

        // Example of usage of the fields.
        std::cout << "[InheritanceClient] Received message " << msg.mid
                << ": \"" << message << "\""
                << " from topic: '" << topic << "'"
                << " size: " << msg.payloadlen
                << " qos: " << msg.qos
                << std::endl;

        // Send back the echoed message.
        publish(OUTPUT_TOPIC, message + " back from InheritanceClient", QoS::QoS0);
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when a disconnection with the MQTT broker occured.
    //! This is called when the broker has received the DISCONNECT command and has
    //! disconnected the client.
    //-------------------------------------------------------------------------
    virtual void onDisconnected(int rc) override
    {
        std::cout << "[InheritanceClient] Disconnected. Reason was '"
                  << Client::error(rc) << "' (" << rc << ")" << std::endl;
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when a message has been sent to the broker
    //! successfully.
    //-------------------------------------------------------------------------
    virtual void onPublished(int mid) override
    {
        std::cout << "[InheritanceClient] Message " << mid << " published" << std::endl;
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when the broker responds to a subscription request.
    //-------------------------------------------------------------------------
    virtual void onSubscribed(int mid, int /*qos_count*/, const int* /*granted_qos*/) override
    {
        std::cout << "[InheritanceClient] Topic " << mid << " subscribed!" << std::endl;
    }

    //-------------------------------------------------------------------------
    //! \brief Callback called when the broker responds to a unsubscription request.
    //-------------------------------------------------------------------------
    virtual void onUnsubscribed(int mid) override
    {
        std::cout << "[InheritanceClient] Message " << mid << " no longer subscribed!"
                  << std::endl;
    }

private:

    Topic INPUT_TOPIC{"Input", false, 0};
    Topic OUTPUT_TOPIC{"Output", false, 0};
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

    LambdaClient(Client::Settings const& settings)
        : m_client(settings)
    {
        static Topic INPUT_TOPIC{"Input", false, 0};
        static Topic OUTPUT_TOPIC{"Output", false, 0};

        // --------------------------------------------------------------------
        auto onMessageReceived = [this](const Message& msg)
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
            m_client.publish(OUTPUT_TOPIC, message + " back from LambdaClient", QoS::QoS0);
        };

        // --------------------------------------------------------------------
        auto onConnected = [this, onMessageReceived](int rc)
        {
            std::cout << "[LambdaClient] Connected to MQTT broker with status '"
                      << Client::error(rc) << "': " << rc << std::endl;

            // Callback reacting to message coming on the topic named "Input".
            // An echoed message is publish to the topic named "Output".
            if (!m_client.subscribe(INPUT_TOPIC, QoS::QoS0, onMessageReceived))
            {
                std::cerr << "[LambdaClient] MQTT subscription failed: "
                          << m_client.error().message() << std::endl;
            }
        };

        // --------------------------------------------------------------------
        std::cout << "[LambdaClient] Versionning:" << std::endl;
        std::cout << "  Mosquitto: "
                  << m_client.version().mosquitto[0] << "."
                  << m_client.version().mosquitto[1] << "."
                  << m_client.version().mosquitto[2] << std::endl;
        std::cout << "  C++ wrapper: "
                  << m_client.version().wrapper[0] << "."
                  << m_client.version().wrapper[1] << "."
                  << m_client.version().wrapper[2] << std::endl;
        std::cout << "  MQTT protocol: "
                  << m_client.version().protocol[0] << "."
                  << m_client.version().protocol[1] << "."
                  << m_client.version().protocol[2] << std::endl;

        // --------------------------------------------------------------------
        // Non blocking connection the MQTT broker. Once the client is connected to
        // the MQTT broker the lambda callback onConnected will be called and we will
        // subscribe to MQTT topics.
        if (!m_client.connect({"localhost", 1883, std::chrono::seconds(60)}, onConnected))
        {
            std::cerr << m_client.error().message() << std::endl;
        }
        else
        {
            m_client.publish(OUTPUT_TOPIC, std::string("Hello from LambdaClient"), QoS::QoS0);
        }
    }

private:

    Client m_client;
};

// *****************************************************************************
//! \brief g++ --std=c++14 -Wall -Wextra -I../include ../src/MQTT.cpp Example.cpp
//! -o example `pkg-config --cflags --libs libmosquitto`
// *****************************************************************************
int main()
{
    LambdaClient client1({"example_lambda_client1", Protocol::V5, Client::Session::Cleanup});
    //InheritanceClient client2({"example_inheritance_client2", Protocol::V5, Client::Session::Cleanup});
    //LambdaClient client3({"example_lambda_client3", Protocol::V311, Client::Session::Cleanup});
    //LambdaClient client4({"example_lambda_client4", Protocol::V31, Client::Session::Cleanup});

    // Client is asynchronous.
    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}