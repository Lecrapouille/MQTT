# MQTT: Class wrapping MQTT mosquitto

A C++11 tiny wrapper class for a MQTT client based on the mosquitto implementation.
You will need small code to make your application running with a MQTT client.

- See https://github.com/eclipse/mosquitto
- See https://www.howtoforge.com/how-to-install-mosquitto-mqtt-message-broker-on-debian-11/

## Hello World example

- In a first Linux console, a subscriber will echo messages from our application. Type:
```
mosquitto_sub -h localhost -t "Example/Output"
```

- In a second Linux console, compile and launch our application. Type:

```
cd example
g++ --std=c++14 -Wall -Wextra -I../include ../src/MQTT.cpp Example.cpp -o example `pkg-config --cflags --libs libmosquitto`
./example
```

- In a third Linux console, a publisher will send messages to our application. Type:
```
mosquitto_pub -h localhost -t "Example/Input" -m "Hello"
```

You will see:
- In the first console:
```
Connected to MQTT broker
MQTT subscribe to 'Example/Input'
Received Message: Hello from Topic: Example/Input
```

- In the first console:
```
Hello
```

Your MQTT client is functional :)
