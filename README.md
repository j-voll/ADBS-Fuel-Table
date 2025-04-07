This project is a comprehensive toolkit for conducting pitch testing on fuel systems. It consists of an Arduino-based control system that precisely manipulates a test platform's pitch angle, collects sensor data, and several Python utilities for data capture and visualization. The system is designed for testing how fuel level and temperature sensors respond during controlled pitch changes.
Key Components
1. Arduino Firmware (sketch_mar26a.ino)
The Arduino sketch controls a motorized actuator to position a test platform at precise pitch angles, interfaces with multiple sensors, and logs real-time data.
Features:

Precision Pitch Control: Automatically adjusts platform to target angles (0°, +5°, -5°)
Multi-sensor Connectivity: Reads from:

WITMotion WT901 inclinometer via Serial2
Reventec LS200-400C fuel level sensor via Serial3
CAN bus interface for temperature sensors


Automated Test Sequence: Performs cyclical testing with:

Stationary periods for measurement stabilization
Precise movement between positions
Configurable number of test cycles


Real-time Data Logging: Outputs properly formatted CSV data at 100Hz

2. Serial Data Capture Utility (capture_serial.py)
A simple Python script that captures the serial output from the Arduino and saves it to timestamped CSV files.

```
# Configure these settings
PORT = 'COM10'  # Change to your Arduino's port
BAUD = 115200
FILENAME = f"test_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
```

3. Data Post-Processing Utility (postprocess.py)
Processes raw CSV data by reformatting values and applying scaling factors to the captured sensor readings.

```
# Process fuel level
try:
    if row[1] != "No Data" and row[1].isdigit():
        fuel_level = int(row[1])
        row[1] = f"{fuel_level / 100:.2f}"
except (ValueError, IndexError):
    pass
```

4. Data Visualization Tool (plotter.py)
Creates sophisticated multi-axis plots showing the relationships between pitch angle, fuel level, and temperature data over time.
Technical Details
Control System Architecture
The Arduino firmware implements a closed-loop control system that:

Reads current pitch from the WITMotion inclinometer
Calculates error from target position (0°, +5°, or -5°)
Actuates motor in appropriate direction
Monitors positioning until target is reached
Captures sensor data during both movement and stationary periods

```
void adjustToPosFivePitch() {
    // ... initialization code ...
    
    // Target is 5 degrees with 0.1 degree tolerance
    while (pitch < 4.9 || pitch > 5.1) {  
        if (pitch > 5.1) {
            // Too high - need to move actuator DOWN
            moveMotorForward();
        } else if (pitch < 4.9) {
            // Too low - need to move actuator UP
            moveMotorBackward();
        }
        
        // Continue logging data during adjustment
        // ... data logging code ...
        
        stopMotor();
        
        // Wait for stabilization while logging data
        // ... stabilization and data logging code ...
    }
}
```

CAN Bus Communication
The system reads fuel and temperature data from CAN bus messages using the MCP2515 controller:

```
CANData readCANData() {
    // ... initialization ...
    
    if (CAN.checkReceive() == CAN_MSGAVAIL) {
        CAN.readMsgBuf(&rxId, &len, rxBuf);
        lastCanMsgTime = millis();  // Update time of last message
        
        // According to spec, one message with all data
        if (len >= 6) {
            // Values are 16-bit integers with MSB first
            uint16_t level = (rxBuf[0] << 8) | rxBuf[1];
            uint16_t internalTemp = (rxBuf[2] << 8) | rxBuf[3];
            uint16_t externalTemp = (rxBuf[4] << 8) | rxBuf[5];
            
            // ... process and store values ...
        }
    }
    
    return lastValidData;
}
```

Data Visualization
The Python plotter creates sophisticated multi-axis charts with phase markers to help analyze how fuel levels and temperatures respond to pitch changes:

```
# Create figure and primary axis for pitch
fig, ax1 = plt.subplots(figsize=(14, 8))

# Plot pitch on primary y-axis
ax1.plot(times, pitches, color='tab:blue', linewidth=2, label='Pitch')

# Create secondary y-axis for fuel level
ax2 = ax1.twinx()
ax2.plot(times, fuel_levels, color='tab:red', linestyle='--', linewidth=2)

# Create third y-axis for temperatures
ax3 = ax1.twinx()
ax3.spines["right"].set_position(("axes", 1.1))
ax3.plot(times, internal_temps, color='tab:green', linestyle='-.', linewidth=2)
ax3.plot(times, external_temps, color='tab:purple', linestyle=':', linewidth=2)
```

Test Procedure

The system initializes and calibrates to 0° pitch
It records baseline data for 5 seconds
For each test cycle (default: 3 cycles):

Adjusts to +5° pitch and holds for 60 seconds while recording data
Adjusts to -5° pitch and holds for 60 seconds while recording data


Returns to 0° pitch position and completes data collection
The captured data can be processed and visualized using the Python utilities

Hardware Requirements

Arduino Mega (or compatible board with multiple hardware serial ports)
L298N motor driver for actuator control
WITMotion WT901 inclinometer
Reventec LS200-400C fuel level sensor
MCP2515 CAN bus interface
Linear actuator with appropriate mounting hardware
Test platform with fuel tank mounting provisions

Usage Instructions

Connect all hardware according to the pinouts defined in the Arduino sketch
Upload the Arduino sketch to the microcontroller
Run capture_serial.py to begin data capture
After test completion, use postprocess.py to normalize the data
Use plotter.py to generate visualization graphs for analysis
