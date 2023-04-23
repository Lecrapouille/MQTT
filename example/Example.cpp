#include "MQTT/MQTT.hpp"

#include <chrono>
#include <thread>
#include <iostream>

using namespace std::chrono_literals;

// *****************************************************************************
//! \brief A MQTT client connecting to the broker (localhost port 1883). Once
//! conncected it will subscribe to the topic "Example/Input" and all messages
//! it will receive will be published to the topic "Example/Output".
// *****************************************************************************
class Example: public MQTT
{
public:

    //-------------------------------------------------------------------------
    //! \brief Default constructor with non blocking connection to the MQTT
    //! broker.
    //-------------------------------------------------------------------------
    Example()
    {
        connect("localhost", 1883); // Connect is not blocking !
    }

private: // MQTT

    //-------------------------------------------------------------------------
    //! \brief Callback when this class is connected to the MQTT broker.
    //-------------------------------------------------------------------------
    virtual void onConnected(int /*rc*/) override
    {
        std::cout << "Connected to MQTT broker" << std::endl;

        subscribe("Example/Input", MQTT::QoS::QoS0);
    }

    //-------------------------------------------------------------------------
    //! \brief Callback when this class is has received a new message from the
    //! MQTT broker.
    //-------------------------------------------------------------------------
    virtual void onMessageReceived(const struct mosquitto_message& msg) override
    {
        std::string message(static_cast<char*>(msg.payload));
        std::string topic(static_cast<char*>(msg.topic));

        std::cout << "Received Message: " << message
                  << " from Topic: " << topic
                  << std::endl;
        publish("Example/Output", message, MQTT::QoS::QoS0);
    }
};

// *****************************************************************************
//! \brief g++ --std=c++14 -Wall -Wextra -I../include ../src/MQTT.cpp Example.cpp
//! -o example `pkg-config --cflags --libs libmosquitto`
// *****************************************************************************
int main()
{
    Example client; // Client is asynchronous.

    while (true)
    {
        std::this_thread::sleep_for(1000ms);
    }
    return 0;
}
