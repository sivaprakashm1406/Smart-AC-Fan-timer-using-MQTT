# Smart-AC-Fan-timer-using-MQTT
Smart energy-saving HVAC automation system using ESP32, MQTT, temperature feedback control, runtime scheduling, and power-consumption analytics.
# Smart Energy Efficient HVAC Automation System

## Project Summary

This project presents a smart HVAC automation solution designed to improve energy efficiency while maintaining user comfort.

Using an ESP32 microcontroller, the system monitors ambient temperature and automatically manages air conditioner operation through intelligent thermostat control. A programmable timer allows scheduled operation, while built-in analytics estimate energy savings achieved through automated shutdown.

---

## Key Objectives

* Reduce unnecessary HVAC operation
* Improve energy efficiency
* Provide real-time environmental monitoring
* Enable remote control through MQTT
* Demonstrate IoT-based building automation

---

## System Architecture

Sensor Layer:

* DHT22 Temperature and Humidity Sensor

Control Layer:

* ESP32 Controller
* Relay-Based AC Switching
* Stepper Motor Driven Fan

User Interface:

* OLED Display
* Physical Timer Buttons

Communication Layer:

* Wi-Fi
* MQTT Broker

Analytics Layer:

* Runtime Tracking
* Energy Saving Estimation

---

## Functional Workflow

1. ESP32 reads environmental conditions.
2. Temperature data is displayed locally.
3. Sensor values are transmitted through MQTT.
4. Thermostat logic determines cooling requirements.
5. AC is activated when temperature exceeds the configured threshold.
6. Fan operates for continuous air circulation.
7. Runtime timer automatically shuts down the system.
8. Energy-saving statistics are calculated and published.

---

## Software Features

### Non-Blocking Architecture

The firmware avoids delay() functions and relies on millis() and micros() timers, enabling:

* Responsive user input
* Reliable MQTT communication
* Smooth multitasking operation

### Smart Thermostat

A hysteresis band of ±0.5°C prevents frequent relay toggling and improves system reliability.

### Remote Configuration

Users can remotely modify:

* Temperature setpoint
* Runtime duration

through MQTT messages.

---

## Energy Optimization

The system estimates energy savings based on:

* AC runtime
* Fan runtime
* Scheduled shutdown duration

This information can be integrated into larger smart-building monitoring systems.

---

## Development Stack

* ESP32
* Arduino IDE
* MQTT
* Wi-Fi
* Embedded C++
* Adafruit SSD1306
* DHT Sensor Library

---

## Potential Applications

* Smart Homes
* Green Buildings
* Classroom Automation
* Hostel Energy Management
* Industrial Climate Monitoring
* IoT Research and Education

---

## Future Scope

* Blynk Dashboard Integration
* Home Assistant Support
* Mobile Notifications
* Predictive Climate Control
* Cloud-Based Analytics
* AI-Assisted Energy Optimization

---

## License

This project is intended for educational, research, and prototyping purposes.
