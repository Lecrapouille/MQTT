# MQTT: Class wrapping MQTT mosquitto

A C++11 tiny wrapper class for a MQTT client based on the mosquitto implementation.
You will need small code to make your application running with a MQTT client.

- See https://github.com/eclipse/mosquitto
- See https://www.howtoforge.com/how-to-install-mosquitto-mqtt-message-broker-on-debian-11/

## Hello World example

- In a first Linux console, a subscriber will echo messages from our application. Type:
```
mosquitto_sub -h localhost -t "Output"
```

- In a second Linux console, compile and launch our application. Type:

```
cd example
g++ --std=c++11 -Wall -Wextra -I../include ../src/MQTT.cpp Example.cpp -o example `pkg-config --cflags --libs libmosquitto`
./example
```

You will see (may be different from your case):
```
Connected to MQTT broker with error code 0
Topic 1 subscribed. Granted QOS: 0
```

- In a third Linux console, a publisher will send messages to our application. Type:
```
mosquitto_pub -h localhost -t "Input" -m "Hello"
```

You will see:
- In the first console:
```
Hello back
```

- In the second console:
```
Received message 0: "Hello" from topic: "Input" size: 5 qos: 0
Message 2 published
```

Your C++ asynchronous MQTT client is functional :)