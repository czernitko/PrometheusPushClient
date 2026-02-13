# PrometheusPushClient Library

The `PrometheusPushClient` library provides an easy way to push metrics from your IoT devices (e.g., ESP32, ESP8266) to a Prometheus Push Gateway. This library is designed to work seamlessly with the ESP32 and ESP8266 platforms using the Arduino framework.

It aims at minimising memory usage and memory fragmentation as much as possible. See the `examples` folder to see how to use it.

## Features

- Push metrics to a Prometheus Push Gateway.
- Support for multiple metric types (e.g., counters, gauges).
- Easy integration with IoT projects.
- Lightweight and efficient for resource-constrained devices.

# Debugging
If you need to see the requests or responses performed by the library, define a port to send the logs to.
For example `DEBUG_PROMETHEUS_PUSH_CLIENT_PORT=Serial` in your `platformio.ini`:
```
[env]
framework = arduino
build_type = release
board_build.filesystem = littlefs
monitor_speed = 115200
platform = espressif8266
board = nodemcuv2
build_flags = 
   -D DEBUG_PROMETHEUS_PUSH_CLIENT_PORT=Serial
lib_deps = 
  czernitko/PrometheusPushClient@^1.0.0
```

## Tips
- Ensure your ESP32/ESP8266 is connected to the network and can reach the Prometheus Push Gateway.
- Make sure the Prometheus Push Gateway is properly configured to accept metrics from your device.
- Monitor your device's memory usage to avoid crashes due to memory constraints.
  