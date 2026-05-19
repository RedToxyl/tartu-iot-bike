import serial

BT_PORT = "COM3" 
BAUD_RATE = 9600

try:
    print(f"Connecting to {BT_PORT}...")
    with serial.Serial(BT_PORT, BAUD_RATE, timeout=1) as bt:
        print("Connected! Listening for events...\n")
        while True:
            raw_data = bt.readline() 
            if raw_data:
                line = raw_data.decode('utf-8', errors='ignore').strip() 
                if "|" in line:
                    topic, payload = line.split("|", 1)
                    print(f"[EVENT] Topic: {topic} | Payload: {payload}")
                elif line:
                    print(f"[RAW] {line}")
                    
except Exception as e:
    print(f"Error: {e}")