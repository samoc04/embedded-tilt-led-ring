# Embedded-tilt-led-ring

STM32 embedded system using accelerometer, ADC, PWM and WS2812 LED ring

![Working Project](working_project.jpeg)

## Overview

This project implements a tilt-controlled LED ring system using an STM32L432 microcontroller and a BMI160 accelerometer. The system detects the orientation of the device in real time and displays the direction using a WS2812B LED ring.

The project demonstrates embedded programming, peripheral interfacing, real-time signal processing, debugging, and iterative system development.

---

## Objectives

The objectives of this project were:

- Interface with an accelerometer using I2C
- Control a WS2812B LED ring using precise timing
- Implement real-time tilt detection
- Map tilt direction to LED position
- Implement filtering for stable output
- Demonstrate use of ADC and timer-related timing control
- Apply structured testing and debugging methods

---

## System Description

The system reads acceleration data from the BMI160 sensor and calculates the direction of tilt using the X and Y axes. This direction is mapped onto a 16-LED WS2812B ring.

### Key features

- A 3-LED pointer indicates tilt direction
- A dead zone prevents jitter when the device is stationary
- A potentiometer allows a position to be saved
- A push button cycles through colour modes
- Saved positions remain visible while live tracking continues
- Filtering improves stability and smoothness of movement

---

## Hardware Used

- STM32L432 Nucleo board
- BMI160 accelerometer
- WS2812B 16-LED ring
- Potentiometer
- Push button
- 330 Ω resistor on LED data line
- Breadboard and jumper wires
- Oscilloscope for signal debugging

---

## Peripherals Used (LO1)

| Peripheral | Purpose |
|---|---|
| GPIO | LED data output, button input |
| I2C | Communication with BMI160 accelerometer |
| ADC | Read potentiometer value |
| Timer / precise timing | WS2812 signal timing and control |
| UART | Debug output to serial terminal |

---

## Software Design

The software was structured into modular functions for:

- sensor reading using I2C
- signal processing and filtering
- LED control using the WS2812 protocol
- button input handling
- potentiometer input handling
- tilt-to-direction mapping

A low-level register-based approach was used rather than relying fully on HAL libraries. This gave finer control over timing, which was especially important for the WS2812 LED protocol.

---

## How the Final System Works

The BMI160 accelerometer provides raw X, Y, and Z acceleration values. The software reads these values over I2C and scales them into usable values. The X and Y axes are then used to determine the tilt direction.

A mapping algorithm compares the measured direction against predefined direction vectors for each LED position on the ring. The best matching LED becomes the centre LED of a 3-LED pointer. This makes the output easy to see and more visually stable than lighting only one LED.

A dead zone is applied so that when the board is close to flat, the pointer stays in a default top position instead of jittering. A low-pass filter is also used to smooth the accelerometer values and reduce noise.

The potentiometer is used as a user input to save a position, and the push button is used to cycle through colour modes.

---

## WS2812 Protocol and DWT Timing

The WS2812B LED ring requires very precise timing on its single data line. Initially, SPI was used to try to emulate the required waveform, but this produced unstable colours and incorrect LED behaviour.

The final working implementation used direct GPIO control together with the DWT cycle counter.

### Why the SPI approach failed

The earlier implementation encoded LED bits into SPI patterns. This approach only works if the SPI clock timing matches the WS2812 timing requirements very closely. In practice, the LEDs interpreted some bits incorrectly, causing unstable colours and skipped LEDs.

### Why the DWT approach worked

The final version drove the data pin directly by setting it high and low in software. The DWT cycle counter was used to measure CPU clock cycles accurately.

At 80 MHz:

- 1 clock cycle = 12.5 ns
- a logic 0 bit is approximately:
  - high for 0.4 µs
  - low for 0.85 µs
- a logic 1 bit is approximately:
  - high for 0.8 µs
  - low for 0.45 µs

Using the DWT cycle counter allowed these timings to be generated accurately, which made the LED ring behave correctly and consistently.

The reset pulse was also implemented properly by holding the line low for more than 50 µs before sending a new LED frame.

---

## Software Development Procedure (LO5)

The system was developed incrementally, with each subsystem tested before integration.

### Stage 1 – Initial LED Control

The first task was to light a single LED on the WS2812 ring. An SPI-based implementation was used initially.

**Result:** the LED output was unstable and colours were inconsistent.

### Stage 2 – LED Indexing and Mapping

Specific LEDs were tested individually to verify ring indexing and physical LED order.

**Result:** some LEDs appeared to be skipped and colours were sometimes incorrect, indicating a protocol/timing issue rather than a mapping issue.

### Stage 3 – Protocol Fix

The SPI-based method was removed and replaced with direct GPIO control using the DWT cycle counter.

**Result:** stable WS2812 communication was achieved.

### Stage 4 – Accelerometer Integration

The BMI160 accelerometer was connected over I2C and raw acceleration values were read successfully.

**Result:** acceleration values were printed over UART and changed correctly with board movement.

### Stage 5 – Basic Tilt Detection

The X-axis was first used to implement basic left/right tilt detection by changing LED colour.

**Result:** tilt could be detected successfully.

### Stage 6 – Multi-Axis Processing

The Y-axis was added so that forward and backward tilt could also be detected. A dominant-axis method was used to avoid conflicting outputs when tilting diagonally.

**Result:** more complete directional control was achieved.

### Stage 7 – Direction Mapping

The project was improved from simple colour changes to a directional LED pointer. A 3-LED pointer was used to indicate tilt direction around the ring.

**Result:** the pointer followed the approximate accelerometer direction.

### Stage 8 – Final Optimisation

A dot-product based direction-matching method and low-pass filtering were added for better direction selection and smoother behaviour.

**Result:** the final system became stable, responsive, and accurate.

---

## Testing and Results (LO4)

Each subsystem was tested independently before full integration.

### Test 1 – LED Communication

**Method:** LEDs were illuminated sequentially.

**Result:** stable output was achieved after replacing SPI control with direct GPIO timing.

### Test 2 – LED Indexing

**Method:** each LED was activated individually to verify its position.

**Result:** all LEDs were mapped correctly after protocol timing was fixed.

### Test 3 – I2C Communication

**Method:** accelerometer register values were read and printed via UART.

**Result:** values updated correctly with tilt.

### Test 4 – Basic Tilt Detection

**Method:** the board was tilted manually left and right.

**Result:** LED output changed correctly according to tilt direction.

### Test 5 – Multi-Axis Direction Detection

**Method:** the board was tilted in multiple directions.

**Result:** different directions were successfully distinguished using X and Y axes.

### Test 6 – Pointer Mapping

**Method:** the board was rotated through a range of orientations.

**Result:** the 3-LED pointer followed the direction of tilt around the ring.

### Test 7 – Filtering

**Method:** filtered and unfiltered motion response were compared.

**Result:** filtering reduced jitter and made the pointer movement smoother.

---

## Debugging Methods

A combination of software and hardware debugging techniques was used.

### Software Debugging

- UART output was used to print raw and filtered accelerometer values
- this confirmed correct I2C communication and scaling
- code was developed in small stages so faults could be isolated more easily

### Ad-hoc Debugging

- LEDs were used to visualise different program states
- individual LED tests were used to verify mapping and indexing
- subsystem-by-subsystem testing helped isolate problems quickly

### Hardware Debugging

- an oscilloscope was used to verify the WS2812 data signal
- the waveform at the LED data pin was observed to confirm pulse timing and signal presence
- this helped identify that the original SPI-based communication method was not producing a reliable WS2812 waveform
- after switching to direct GPIO timing, the oscilloscope confirmed that valid data pulses were present at the LED ring data input

---

## Circuit Design

A schematic was created using KiCad.

Connections include:

- I2C lines SDA and SCL connected to the BMI160
- GPIO output connected to the WS2812B LED ring data input through a 330 Ω resistor
- potentiometer connected to an ADC input pin
- push button connected to a GPIO input with pull-up configuration
- common ground shared between microcontroller, sensor, and LED ring

[View Full Schematic PDF](Circuit_schematic.pdf)

---

## System Images

### Working Project
![Working Project](working_project.jpeg)

### Initial LED Ring Configuration
![Initial LED Ring Configuration](initial_led_ring_config.jpeg)

### LED Colour Configuration
![LED Colour Configuration](led_colour_configuration.jpeg)

### LED Configuration
![LED Configuration](Led_configuration.jpeg)

### Incorrect Protocol Debugging
![Incorrect Protocol Debugging](Led_incorrect_protocol_debuging.jpeg)

### Oscilloscope Debug of Data Pulse
![Oscilloscope Debug](Hardware_debug_datapulsefor_ledring.jpeg)

---

## Development Log

A more detailed record of code development, debugging, and testing is available here:

[View Full Development Log](Development_Log.docx)

---

## Code Evolution Summary

The code evolved through several important versions:

- **Version 1:** initial SPI-based WS2812 LED control
- **Version 2:** individual LED addressing and LED mapping tests
- **Version 3:** direct GPIO-based WS2812 implementation using DWT timing
- **Version 4:** I2C communication with BMI160 and UART debug output
- **Version 5:** basic X-axis tilt detection with colour-based output
- **Version 6:** X and Y axis tilt handling with dominant-axis logic
- **Version 7:** angle-based direction pointer
- **Version 8:** filtered direction mapping with improved stability and final system behaviour

This development process showed the progression from early proof-of-concept code to a stable final implementation.

---

## Results Summary

The final system:

- accurately detects tilt direction
- displays direction using a 3-LED pointer on the WS2812 ring
- provides stable output using filtering and a dead zone
- allows user interaction through a button and potentiometer
- demonstrates successful integration of multiple embedded peripherals

The final system is responsive, stable, and meets the original project objectives.

---

## Video Demonstration

[Watch the demo video on YouTube](https://youtu.be/A2sVNCb3_ys)
