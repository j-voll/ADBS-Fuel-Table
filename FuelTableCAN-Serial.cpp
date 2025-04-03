#include <SPI.h>
#include <mcp_can.h>
#include <Wire.h>

#define MOTOR_ENA 9   //Enable pin for L298N
#define MOTOR_IN1 8   //Input 1 for L298N
#define MOTOR_IN2 7   //Input 2 for L298N

#define CAN_CS 10      //Chip Select for MCP2515 CAN module

#define WT901_SERIAL Serial2  //WITMotion WT901 connected to Serial2
#define FUEL_SERIAL Serial3   //Reventec LS200-400C connected to Serial3

//Constant definitions for data logging
const unsigned long DATA_INTERVAL = 10;  //Stream data every 10ms (100Hz)

//Structure to hold CAN data
struct CANData {
    String fuelLevel;
    String internalTemp;
    String externalTemp;
    bool externalSensorValid;
};

MCP_CAN CAN(CAN_CS);

bool isMoving = false;
unsigned long lastCanMsgTime = 0;  //To track last CAN message
unsigned long startTime = 0;       //For calculating elapsed time
bool testComplete = false;         //Flag to indicate test completion
bool headersWritten = false;       //Flag to track if CSV headers have been written

//Function declarations
void moveMotorForward();
void moveMotorBackward();
void stopMotor();
float readPitch();
void adjustToZeroPitch();
void returnToZeroPitch();
void adjustToPosFivePitch();
void adjustToPosTenPitch();
void adjustToNegFivePitch();
void adjustToNegTenPitch();
CANData readCANData();
void streamCSVData(const char* phase, const char* direction);

void setup() {
    Serial.begin(115200);  //Debugging output
    while (!Serial && millis() < 3000) {
        ; //Wait for serial port to connect (needed for native USB port only)
    }
    
    //All debug messages go to Serial1, keeping main Serial clean for CSV & Data Parsing
    Serial1.begin(115200);
    Serial1.println("# Test Data Collection Starting");
    Serial1.println("# Initializing system...");
    
    WT901_SERIAL.begin(9600);  //WT901 default baud rate
    FUEL_SERIAL.begin(9600);  //Fuel sensor default baud rate

    pinMode(MOTOR_ENA, OUTPUT);
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    
    pinMode(CAN_CS, OUTPUT);
    digitalWrite(CAN_CS, HIGH);  //Make sure CAN CS is high when not in use
    
    delay(100);

    //Initialize CAN with more detailed error reporting
    Serial1.println("# Initializing CAN bus...");
    
    //1.0 Mbps baud rate
    byte canStatus = CAN.begin(MCP_ANY, CAN_1000KBPS, MCP_8MHZ);
    
    if (canStatus == CAN_OK) {
        Serial1.println("# CAN module initialized successfully at 1Mbps");
    } else {
        Serial1.print("# CAN module initialization failed at 1Mbps. Error code: ");
        Serial1.println(canStatus);
        Serial1.println("# Trying alternate baud rates...");
        
        //Try other common baud rates
        byte baudRates[] = {CAN_500KBPS, CAN_250KBPS, CAN_125KBPS};
        const char* baudRateNames[] = {"500kbps", "250kbps", "125kbps"};
        
        for (int i = 0; i < 3; i++) {
            delay(100);
            canStatus = CAN.begin(MCP_ANY, baudRates[i], MCP_8MHZ);
            if (canStatus == CAN_OK) {
                Serial1.print("# CAN module initialized at ");
                Serial1.println(baudRateNames[i]);
                break;
            }
        }
        
        if (canStatus != CAN_OK) {
            Serial1.println("# CAN module initialization failed with all settings");
        }
    }
    
    //Set CAN module to normal operation mode
    CAN.setMode(MCP_NORMAL);
    
    //Configure masks and filters
    CAN.init_Mask(0, 0, 0x00000000);  //Mask 0 - allow all IDs
    CAN.init_Mask(1, 0, 0x00000000);  //Mask 1 - allow all IDs
    
    for (byte i = 0; i < 6; i++) {
        CAN.init_Filt(i, 0, 0x00000000);  //Filter i - allow all IDs
    }

    //Headers will be written before data collection starts
    headersWritten = false;

    //Stop motor at startup
    stopMotor();
    
    Serial1.println("# Adjusting actuator to achieve 0-degree pitch...");
    adjustToZeroPitch();
    Serial1.println("# Pitch is now 0 degrees. Starting test motion...");
    
    //Record start time for elapsed time calculations
    startTime = millis();
    testComplete = false;
}

void loop() {
    //If test is complete, enter idle state
    if (testComplete) {
        //Check if reset is requested via serial command
        if (Serial.available() > 0) {
            String command = Serial.readStringUntil('\n');
            if (command == "reset") {
                Serial1.println("# Resetting system...");
                setup();  //Call setup to reset the system
                return;
            }
        }
        delay(100);  //Small delay to prevent busy-waiting
        return;      //Skip the rest of the loop
    }
    
    //Begin data streaming
    unsigned long cycleStartTime = millis();
    unsigned long lastDataTime = 0;
    unsigned long movementEndTime;
    
    //Write headers before starting data collection
    if (!headersWritten) {
        Serial.println("TimeMS,FuelLevel,InternalTemp,ExternalTemp,Pitch,Phase,MovementDirection");
        headersWritten = true;
    }
    
    //Pitch to Positive 5
    Serial1.println("# Raising to +5");
    adjustToPosFivePitch();
   
    //Print warning if no CAN messages received
    if (millis() - lastCanMsgTime > 60000) {  //If no CAN messages for 60 seconds
        Serial1.println("# WARNING: No CAN messages received in the last 60 seconds.");
        lastCanMsgTime = millis();  //Reset to avoid repeated warnings
    }
    
    stopMotor();
    
    Serial1.println("# Starting first stationary period");
    //Continue collecting data during first stationary period
    movementEndTime = millis() + 10000;
    lastDataTime = millis();
    while (millis() < movementEndTime) {
        if (millis() - lastDataTime >= DATA_INTERVAL) {
            streamCSVData("Stationary1", "None");
            lastDataTime = millis();
        }
    }
    
    //Pitch to Negative 5
    Serial1.println("# Lowering to -5");
    adjustToNegFivePitch();
   
    //Print warning if no CAN messages received
    if (millis() - lastCanMsgTime > 60000) {  //If no CAN messages for 60 seconds
        Serial1.println("# WARNING: No CAN messages received in the last 60 seconds.");
        lastCanMsgTime = millis();  //Reset to avoid repeated warnings
    }
    
    stopMotor();
    
    Serial1.println("# Starting second stationary period");
    //Continue collecting data during final stationary period
    movementEndTime = millis() + 10000;
    lastDataTime = millis();
    while (millis() < movementEndTime) {
        if (millis() - lastDataTime >= DATA_INTERVAL) {
            streamCSVData("Stationary2", "None");
            lastDataTime = millis();
        }
    }
    
    //Pitch to Positive 10
    Serial1.println("# Raising to +10");
    adjustToPosTenPitch();
   
    //Print warning if no CAN messages received
    if (millis() - lastCanMsgTime > 60000) {  //If no CAN messages for 60 seconds
        Serial1.println("# WARNING: No CAN messages received in the last 60 seconds.");
        lastCanMsgTime = millis();  //Reset to avoid repeated warnings
    }
    
    stopMotor();
    
    Serial1.println("# Starting third stationary period");
    //Continue collecting data during final stationary period
    movementEndTime = millis() + 10000;
    lastDataTime = millis();
    while (millis() < movementEndTime) {
        if (millis() - lastDataTime >= DATA_INTERVAL) {
            streamCSVData("Stationary3", "None");
            lastDataTime = millis();
        }
    }
       
    //Pitch to Negative 10
    Serial1.println("# Lowering to -10");
    adjustToNegTenPitch();  
   
    //Print warning if no CAN messages received
    if (millis() - lastCanMsgTime > 60000) {  //If no CAN messages for 60 seconds
        Serial1.println("# WARNING: No CAN messages received in the last 60 seconds.");
        lastCanMsgTime = millis();  //Reset to avoid repeated warnings
    }
    
    stopMotor();
    
    Serial1.println("# Starting fourth stationary period");
    //Continue collecting data during final stationary period
    movementEndTime = millis() + 10000;
    lastDataTime = millis();
    while (millis() < movementEndTime) {
        if (millis() - lastDataTime >= DATA_INTERVAL) {
            streamCSVData("Stationary4", "None"); 
            lastDataTime = millis();
        }
    }
    
    //Return to zero pitch position
    Serial1.println("# Returning to zero pitch position");
    returnToZeroPitch();
    
    Serial1.println("# Test cycle complete - System waiting for reset");
    Serial1.println("# Send 'reset' command to begin a new test");
    testComplete = true;  //Set flag to stop further testing until reset
    
    //Print warning if no CAN messages received
    if (millis() - lastCanMsgTime > 60000) {  //If no CAN messages for 60 seconds
        Serial1.println("# WARNING: No CAN messages received in the last 60 seconds.");
        lastCanMsgTime = millis();  //Reset to avoid repeated warnings
    }
}

void moveMotorForward() {
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    analogWrite(MOTOR_ENA, 255);
    isMoving = true;
}

void moveMotorBackward() {
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, HIGH);
    analogWrite(MOTOR_ENA, 255);
    isMoving = true;
}

void stopMotor() {
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    analogWrite(MOTOR_ENA, 0);
    isMoving = false;
}

float readPitch() {
    if (WT901_SERIAL.available() >= 11) {  //Make sure we have enough bytes for a complete packet
        byte header = WT901_SERIAL.read();
        if (header == 0x55) {
            byte buffer[10];
            buffer[0] = header;
            
            //Read the remaining 9 bytes of the packet
            WT901_SERIAL.readBytes(&buffer[1], 9);
            
            if (buffer[1] == 0x53) {  //Check if it's an angle packet
                int16_t pitch_raw = (buffer[3] << 8) | buffer[2];
                return pitch_raw / 32768.0 * 180.0;
            }
        }
    }
    
    //If -999, not enough data, or wrong packets. Don't clear buffer.
    return -999.0;
}

void adjustToZeroPitch() {
    float pitch = -999.0;
    int timeoutCounter = 0;
    const int maxTimeout = 1000;  //Maximum number of attempts to read valid pitch
    
    //Try to get a valid pitch reading in the range of -25 to +25 degrees
    while ((pitch == -999.0 || pitch < -25.0 || pitch > 25.0) && timeoutCounter < maxTimeout) {
        pitch = readPitch();
        if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
            delay(10);  //Short delay before trying again
            timeoutCounter++;
        }
    }
    
    if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
        Serial1.println("# Failed to get valid pitch reading. Check inclinometer connection.");
        return;
    }
    
    Serial1.print("# Initial pitch: ");
    Serial1.println(pitch);
    
    while (abs(pitch) > 0.1) {
        if (pitch > 0) {
            //Positive pitch - need to move actuator DOWN
            Serial1.println("# Moving actuator DOWN");
            moveMotorForward();  //Assuming "forward" means DOWN
        } else {
            //Negative pitch - need to move actuator UP
            Serial1.println("# Moving actuator UP");
            moveMotorBackward();  //Assuming "backward" means UP
        }
        
        delay(200);  //Give time for actuator to move
        stopMotor();
        delay(1000);   //Short pause to let system stabilize
        
        //Try to get a valid pitch reading after movement
        timeoutCounter = 0;
        pitch = -999.0;
        
        while ((pitch == -999.0 || pitch < -25.0 || pitch > 25.0) && timeoutCounter < maxTimeout) {
            pitch = readPitch();
            if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
                delay(10);
                timeoutCounter++;
            }
        }
        
        if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
            Serial1.println("# Lost valid pitch reading during adjustment. Stopping.");
            break;
        }
        
        Serial1.print("# Current pitch: ");
        Serial1.println(pitch);
    }
    
    stopMotor();
    Serial1.println("# Pitch stabilized at near 0 degrees.");
}

//Return to zero pitch at the end of the test
void returnToZeroPitch() {
    float pitch = -999.0;
    int timeoutCounter = 0;
    const int maxTimeout = 1000;
    unsigned long lastDataTime = 0;
    
    //Get current pitch
    while ((pitch == -999.0 || pitch < -25.0 || pitch > 25.0) && timeoutCounter < maxTimeout) {
        pitch = readPitch();
        if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
            delay(10);
            timeoutCounter++;
        }
    }
    
    if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
        Serial1.println("# Failed to get valid pitch reading during return to zero.");
        return;
    }
    
    Serial1.print("# Return to zero - starting pitch: ");
    Serial1.println(pitch);
    
    //Adjust to zero
    while (abs(pitch) > 0.1) {
        if (pitch > 0) {
            Serial1.println("# Return to zero - moving actuator DOWN");
            moveMotorForward();
        } else {
            Serial1.println("# Return to zero - moving actuator UP");
            moveMotorBackward();
        }
        
        //Continue logging data during adjustment
        unsigned long adjustStartTime = millis();
        lastDataTime = millis();
        while (millis() < adjustStartTime + 200) {  //200ms adjustment time
            if (millis() - lastDataTime >= DATA_INTERVAL) {
                streamCSVData("ReturnToZero", (pitch > 0) ? "Down" : "Up");
                lastDataTime = millis();
            }
        }
        
        stopMotor();
        
        //Log data during stabilization
        adjustStartTime = millis();
        lastDataTime = millis();
        while (millis() < adjustStartTime + 1000) {  //1000ms stabilization time
            if (millis() - lastDataTime >= DATA_INTERVAL) {
                streamCSVData("ReturnToZero", "Stabilizing");
                lastDataTime = millis();
            }
        }
        
        //Get new pitch reading
        timeoutCounter = 0;
        pitch = -999.0;
        
        while ((pitch == -999.0 || pitch < -25.0 || pitch > 25.0) && timeoutCounter < maxTimeout) {
            pitch = readPitch();
            if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
                delay(10);
                timeoutCounter++;
            }
        }
        
        if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
            Serial1.println("# Lost valid pitch reading during return to zero. Stopping.");
            break;
        }
        
        Serial1.print("# Return to zero - current pitch: ");
        Serial1.println(pitch);
    }
    
    stopMotor();
    
    //Log final data points
    lastDataTime = millis();
    for (int i = 0; i < 100; i++) {  //Log 100 more data points at zero position
        if (millis() - lastDataTime >= DATA_INTERVAL) {
            streamCSVData("Complete", "Zero");
            lastDataTime = millis();
        }
        delay(10);
    }
    
    Serial1.println("# Return to zero complete - pitch stabilized at zero degrees.");
}


//Raise to positive five degrees
void adjustToPosFivePitch() {
    float pitch = -999.0;
    int timeoutCounter = 0;
    const int maxTimeout = 1000;  //Maximum number of attempts to read valid pitch
    unsigned long lastDataTime = 0;
    
    //Try to get a valid pitch reading in the range of -25 to +25 degrees
    while ((pitch == -999.0 || pitch < -25.0 || pitch > 25.0) && timeoutCounter < maxTimeout) {
        pitch = readPitch();
        if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
            delay(10);  //Short delay before trying again
            timeoutCounter++;
        }
    }
    
    if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
        Serial1.println("# Failed to get valid pitch reading. Check inclinometer connection.");
        return;
    }
    
    Serial1.print("# Initial pitch: ");
    Serial1.println(pitch);
    
    while (pitch < 4.9 || pitch > 5.1) {  //Target is 5 degrees with 0.1 degree tolerance
        if (pitch > 5.1) {
            //Too high - need to move actuator DOWN
            Serial1.println("# Moving actuator DOWN");
            moveMotorForward();  //Forward=DOWN
        } else if (pitch < 4.9) {
            //Too low - need to move actuator UP
            Serial1.println("# Moving actuator UP");
            moveMotorBackward();  //Backward=UP
        }
        
        //Continue logging data during adjustment
        unsigned long adjustStartTime = millis();
        lastDataTime = millis();
        while (millis() < adjustStartTime + 200) {  //200ms adjustment time
            if (millis() - lastDataTime >= DATA_INTERVAL) {
                streamCSVData("AdjustingToPos5", (pitch > 5.1) ? "Down" : "Up");
                lastDataTime = millis();
            }
        }
        
        stopMotor();
        
        //Wait for stabilization while logging data
        adjustStartTime = millis();
        lastDataTime = millis();
        while (millis() < adjustStartTime + 1000) {  //1000ms stabilization time
            if (millis() - lastDataTime >= DATA_INTERVAL) {
                streamCSVData("AdjustingToPos5", "Stabilizing");
                lastDataTime = millis();
            }
        }
        
        //Try to get a valid pitch reading after movement
        timeoutCounter = 0;
        pitch = -999.0;
        
        while ((pitch == -999.0 || pitch < -25.0 || pitch > 25.0) && timeoutCounter < maxTimeout) {
            pitch = readPitch();
            if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
                delay(10);
                timeoutCounter++;
            }
        }
        
        if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
            Serial1.println("# Lost valid pitch reading during adjustment. Stopping.");
            break;
        }
        
        Serial1.print("# Current pitch: ");
        Serial1.println(pitch);
    }
    
    stopMotor();
    Serial1.println("# Pitch stabilized at near +5 degrees.");
}


//Raise to positive 10 degrees
void adjustToPosTenPitch() {
    float pitch = -999.0;
    int timeoutCounter = 0;
    const int maxTimeout = 1000;  //Maximum number of attempts to read valid pitch
    unsigned long lastDataTime = 0;
    
    //Try to get a valid pitch reading in the range of -25 to +25 degrees
    while ((pitch == -999.0 || pitch < -25.0 || pitch > 25.0) && timeoutCounter < maxTimeout) {
        pitch = readPitch();
        if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
            delay(10);  //Short delay before trying again
            timeoutCounter++;
        }
    }
    
    if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
        Serial1.println("# Failed to get valid pitch reading. Check inclinometer connection.");
        return;
    }
    
    Serial1.print("# Initial pitch: ");
    Serial1.println(pitch);
    
    //Fix the logic for positive 10 degrees
    while (pitch < 9.9 || pitch > 10.1) {  //Target is 10 degrees with 0.1 degree tolerance
        if (pitch > 10.1) {
            //Too high - need to move actuator DOWN
            Serial1.println("# Moving actuator DOWN");
            moveMotorForward();  //Forward=DOWN
        } else if (pitch < 9.9) {
            //Too low - need to move actuator UP
            Serial1.println("# Moving actuator UP");
            moveMotorBackward();  //Backward=UP
        }

        //Continue logging data during adjustment
        unsigned long adjustStartTime = millis();
        lastDataTime = millis();
        while (millis() < adjustStartTime + 200) {  //200ms adjustment time
            if (millis() - lastDataTime >= DATA_INTERVAL) {
                streamCSVData("AdjustingToPos10", (pitch > 10.1) ? "Down" : "Up");
                lastDataTime = millis();
            }
        }
        
        stopMotor();
        
        //Wait for stabilization while logging data
        adjustStartTime = millis();
        lastDataTime = millis();
        while (millis() < adjustStartTime + 1000) {  //1000ms stabilization time
            if (millis() - lastDataTime >= DATA_INTERVAL) {
                streamCSVData("AdjustingToPos10", "Stabilizing");
                lastDataTime = millis();
            }
        }
        
        //Try to get a valid pitch reading after movement
        timeoutCounter = 0;
        pitch = -999.0;
        
        while ((pitch == -999.0 || pitch < -25.0 || pitch > 25.0) && timeoutCounter < maxTimeout) {
            pitch = readPitch();
            if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
                delay(10);
                timeoutCounter++;
            }
        }
        
        if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
            Serial1.println("# Lost valid pitch reading during adjustment. Stopping.");
            break;
        }
        
        Serial1.print("# Current pitch: ");
        Serial1.println(pitch);
    }
    
    stopMotor();
    Serial1.println("# Pitch stabilized at near +10 degrees.");
}

//Adjust to negative five degrees
void adjustToNegFivePitch() {
    float pitch = -999.0;
    int timeoutCounter = 0;
    const int maxTimeout = 1000;  //Maximum number of attempts to read valid pitch
    unsigned long lastDataTime = 0;
    
    //Try to get a valid pitch reading in the range of -25 to +25 degrees
    while ((pitch == -999.0 || pitch < -25.0 || pitch > 25.0) && timeoutCounter < maxTimeout) {
        pitch = readPitch();
        if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
            delay(10);  //Short delay before trying again
            timeoutCounter++;
        }
    }
    
    if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
        Serial1.println("# Failed to get valid pitch reading. Check inclinometer connection.");
        return;
    }
    
    Serial1.print("# Initial pitch: ");
    Serial1.println(pitch);
  
    while (pitch < -5.1 || pitch > -4.9) {  //Target is -5 degrees with 0.1 degree tolerance
        if (pitch < -5.1) {
            //Too low - need to move actuator UP
            Serial1.println("# Moving actuator UP");
            moveMotorBackward();  //Backward=UP
        } else if (pitch > -4.9) {
            //Too high - need to move actuator DOWN
            Serial1.println("# Moving actuator DOWN");
            moveMotorForward();  //Forward=DOWN
        }
        
        //Continue logging data during adjustment
        unsigned long adjustStartTime = millis();
        lastDataTime = millis();
        while (millis() < adjustStartTime + 200) {  //200ms adjustment time
            if (millis() - lastDataTime >= DATA_INTERVAL) {
                streamCSVData("AdjustingToNeg5", (pitch < -5.1) ? "Up" : "Down");
                lastDataTime = millis();
            }
        }
        
        stopMotor();
        
        //Wait for stabilization while logging data
        adjustStartTime = millis();
        lastDataTime = millis();
        while (millis() < adjustStartTime + 1000) {  //1000ms stabilization time
            if (millis() - lastDataTime >= DATA_INTERVAL) {
                streamCSVData("AdjustingToNeg5", "Stabilizing");
                lastDataTime = millis();
            }
        }
        
        //Try to get a valid pitch reading after movement
        timeoutCounter = 0;
        pitch = -999.0;
        
        while ((pitch == -999.0 || pitch < -25.0 || pitch > 25.0) && timeoutCounter < maxTimeout) {
            pitch = readPitch();
            if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
                delay(10);
                timeoutCounter++;
            }
        }
        
        if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
            Serial1.println("# Lost valid pitch reading during adjustment. Stopping.");
            break;
        }
        
        Serial1.print("# Current pitch: ");
        Serial1.println(pitch);
    }
    
    stopMotor();
    Serial1.println("# Pitch stabilized at near -5 degrees.");
}

//Adjust to negative ten degrees
void adjustToNegTenPitch() {
    float pitch = -999.0;
    int timeoutCounter = 0;
    const int maxTimeout = 1000;  //Maximum number of attempts to read valid pitch
    unsigned long lastDataTime = 0;
    
    //Try to get a valid pitch reading in the range of -25 to +25 degrees
    while ((pitch == -999.0 || pitch < -25.0 || pitch > 25.0) && timeoutCounter < maxTimeout) {
        pitch = readPitch();
        if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
            delay(10);  //Short delay before trying again
            timeoutCounter++;
        }
    }
    
    if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
        Serial1.println("# Failed to get valid pitch reading. Check inclinometer connection.");
        return;
    }
    
    Serial1.print("# Initial pitch: ");
    Serial1.println(pitch);
    
    while (pitch < -10.1 || pitch > -9.9) {  //Target is -10 degrees with 0.1 degree tolerance
        if (pitch < -10.1) {
            //Too low - need to move actuator UP
            Serial1.println("# Moving actuator UP");
            moveMotorBackward();  //Backward=UP
        } else if (pitch > -9.9) {
            //Too high - need to move actuator DOWN
            Serial1.println("# Moving actuator DOWN");
            moveMotorForward();  //Forward=DOWN
        }
        
        //Continue logging data during adjustment
        unsigned long adjustStartTime = millis();
        lastDataTime = millis();
        while (millis() < adjustStartTime + 200) {  //200ms adjustment time
            if (millis() - lastDataTime >= DATA_INTERVAL) {
                streamCSVData("AdjustingToNeg10", (pitch < -10.1) ? "Up" : "Down");
                lastDataTime = millis();
            }
        }
        
        stopMotor();
        
        //Wait for stabilization while logging data
        adjustStartTime = millis();
        lastDataTime = millis();
        while (millis() < adjustStartTime + 1000) {  //1000ms stabilization time
            if (millis() - lastDataTime >= DATA_INTERVAL) {
                streamCSVData("AdjustingToNeg10", "Stabilizing");
                lastDataTime = millis();
            }
        }
        
        //Try to get a valid pitch reading after movement
        timeoutCounter = 0;
        pitch = -999.0;
        
        while ((pitch == -999.0 || pitch < -25.0 || pitch > 25.0) && timeoutCounter < maxTimeout) {
            pitch = readPitch();
            if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
                delay(10);
                timeoutCounter++;
            }
        }
        
        if (pitch == -999.0 || pitch < -25.0 || pitch > 25.0) {
            Serial1.println("# Lost valid pitch reading during adjustment. Stopping.");
            break;
        }
        
        Serial1.print("# Current pitch: ");
        Serial1.println(pitch);
    }
    
    stopMotor();
    Serial1.println("# Pitch stabilized at near -10 degrees.");
}

//Read CAN data
CANData readCANData() {
    static CANData lastValidData = {"No Data", "No Data", "No Data", false};
    long unsigned int rxId;
    unsigned char len = 0;
    unsigned char rxBuf[8];
    
    if (CAN.checkReceive() == CAN_MSGAVAIL) {
        CAN.readMsgBuf(&rxId, &len, rxBuf);
        lastCanMsgTime = millis();  //Update the time of last message
      
        if (len >= 6) {
            //Values are 16-bit integers with MSB first
            uint16_t level = (rxBuf[0] << 8) | rxBuf[1];
            uint16_t internalTemp = (rxBuf[2] << 8) | rxBuf[3];
            uint16_t externalTemp = (rxBuf[4] << 8) | rxBuf[5];
            
            //Set the values to the structure
            lastValidData.fuelLevel = String(level);
            lastValidData.internalTemp = String(internalTemp);
            
            //Check for external temp sensor status
            if (externalTemp == 0xFFFF) {
                lastValidData.externalTemp = "Disabled";
                lastValidData.externalSensorValid = false;
            } else if (externalTemp == 0x8001) {
                lastValidData.externalTemp = "Open Circuit";
                lastValidData.externalSensorValid = false;
            } else if (externalTemp == 0x8002) {
                lastValidData.externalTemp = "Short Circuit";
                lastValidData.externalSensorValid = false;
            } else {
                lastValidData.externalTemp = String(externalTemp);
                lastValidData.externalSensorValid = true;
            }
        }
    }
    
    return lastValidData;
}

//Stream data in CSV format to Serial Monitor (in this use case, see serial_capture.py)
void streamCSVData(const char* phase, const char* direction) {
    float pitch = readPitch();
    CANData canData = readCANData();
    unsigned long elapsedTime = millis() - startTime;
    
    //Only proceed if pitch is in valid range (-25 to +25 degrees)
    if (pitch != -999.0 && pitch >= -25.0 && pitch <= 25.0) {
        //Formatted for CSV
        Serial.print(elapsedTime);
        Serial.print(",");
        Serial.print(canData.fuelLevel);
        Serial.print(",");
        Serial.print(canData.internalTemp);
        Serial.print(",");
        Serial.print(canData.externalTemp);
        Serial.print(",");
        Serial.print(pitch, 2);
        Serial.print(",");
        Serial.print(phase);
        Serial.print(",");
        Serial.println(direction);
        
        if (Serial1) {
            //Every 100 data points (approx. 1 second), print a debug status
            if (elapsedTime % 1000 < 10) {
                Serial1.print("# Status at ");
                Serial1.print(elapsedTime);
                Serial1.print("ms: Phase=");
                Serial1.print(phase);
                Serial1.print(", Direction=");
                Serial1.print(direction);
                Serial1.print(", Pitch=");
                Serial1.print(pitch);
                Serial1.print(", Fuel=");
                Serial1.print(canData.fuelLevel);
                Serial1.print(", Temp=");
                Serial1.println(canData.internalTemp);
            }
        }
    }
}
