import serial
import time
from datetime import datetime

# Configure these settings
PORT = 'COM10'  # Change to your Arduino's port
BAUD = 115200
FILENAME = f"test_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"

# Open serial connection
ser = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(2)  # Allow connection to establish

print(f"Starting data capture to {FILENAME}")
print("Press Ctrl+C to stop")

try:
    with open(FILENAME, 'w') as file:
        while True:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8').strip()
                print(line)  # Echo to console
                file.write(line + '\n')
                file.flush()  # Make sure data is written immediately
except KeyboardInterrupt:
    print("\nCapture stopped")
finally:
    ser.close()
    print(f"Data saved to {FILENAME}")
