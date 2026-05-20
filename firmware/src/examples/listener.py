import serial
import time

BT_PORT = "COM5" 
BAUD_RATE = 9600 

def listen_to_bluetooth():
    print(f"Starting Bluetooth listener on {BT_PORT}...")
    
    while True:
        try:
            with serial.Serial(BT_PORT, BAUD_RATE, timeout=1) as bt:
                print(f"✅ Connected to {BT_PORT}! Requesting state sync...\n")
                
                bt.reset_input_buffer()
                
                bt.write(b"SYNC\n")
                
                while True:
                    raw_data = bt.readline() 
                    
                    if raw_data and raw_data.endswith(b'\n'):
                        line = raw_data.decode('utf-8', errors='replace').strip() 
                        
                        if "|" in line:
                            try:
                                topic, payload = line.split("|", 1)
                                topic = topic.strip()
                                payload = payload.strip()
                                
                                if topic == "SYSTEM" and payload == "SYNC_COMPLETE":
                                    print("✅ --- STATE SYNC COMPLETE ---")
                                else:
                                    print(f"[DATA] Topic: {topic} | Payload: {payload}")
                                    
                            except ValueError:
                                print(f"[MALFORMED] {line}")
                        elif line:
                            print(f"[RAW] {line}")
                            
        except serial.SerialException as e:
            print(f"⚠️ Connection lost. Retrying in 3 seconds...")
            time.sleep(3)
            
        except KeyboardInterrupt:
            print("\n🛑 Exiting gracefully. Goodbye!")
            break
            
        except OSError as e:
            print(f"❌ OS Error: {e}. Retrying in 3 seconds...")
            time.sleep(3)

if __name__ == "__main__":
    listen_to_bluetooth()