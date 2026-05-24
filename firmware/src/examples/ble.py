import asyncio
from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

DEVICE_NAME = "ESP_MESH_BRIDGE"
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

def notification_handler(sender, data):
    """Fired automatically whenever the BLE Bridge broadcasts data."""
    # Decode the byte array back into a string
    line = data.decode('utf-8', errors='replace').strip()
    if not line:
        return
        
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
    else:
        print(f"[RAW] {line}")


async def run_client():
    print("🚀 Starting BLE Auto-Sync Client...")
    
    while True:
        print(f"\n🔍 Scanning for '{DEVICE_NAME}' or Service UUID '{SERVICE_UUID}'...")
        
        try:
            # return_adv=True tells Bleak to return a dictionary containing both the Device and its Advertisement Data
            devices_dict = await BleakScanner.discover(timeout=5.0, return_adv=True)
            target_device = None
            
            # Iterate through the dictionary values
            for address, (device, adv_data) in devices_dict.items():
                
                # 1. Try matching by Name
                if device.name == DEVICE_NAME:
                    target_device = device
                    break
                    
                # 2. Try matching by Service UUID (Bypasses OS name caching!)
                advertised_uuids = [uuid.lower() for uuid in adv_data.service_uuids]
                if SERVICE_UUID.lower() in advertised_uuids:
                    target_device = device
                    break
            
            if not target_device:
                print("⏳ Not found. Devices currently broadcasting in your area:")
                for address, (device, adv_data) in devices_dict.items():
                    # Print found devices to help you debug what the OS is calling it
                    name_to_print = device.name if device.name else "Unknown"
                    print(f"   - {name_to_print} ({address}) | UUIDs: {adv_data.service_uuids}")
                
                print("Retrying in 3 seconds...\n")
                await asyncio.sleep(3)
                continue
                
            print(f"🔗 Found bridge at {target_device.address}! Connecting...")
            
            async with BleakClient(target_device) as client:
                print("✅ Connected!")
                
                # Subscribe to the data stream
                await client.start_notify(CHAR_UUID, notification_handler)
                
                # Ask the Master Node for a state sync!
                print("🔄 Requesting Master Node SYNC...")
                sync_cmd = b"SYNC\n"
                await client.write_gatt_char(CHAR_UUID, sync_cmd)
                
                # Keep the script alive while connected
                while client.is_connected:
                    await asyncio.sleep(1)
                    
            print("🔌 Disconnected.")
            
        except BleakError as e:
            print(f"⚠️ BLE Error: {e}")
            await asyncio.sleep(2)
        except Exception as e:
            print(f"❌ Unexpected Error: {e}")
            await asyncio.sleep(2)


if __name__ == "__main__":
    try:
        asyncio.run(run_client())
    except KeyboardInterrupt:
        print("\n🛑 Exiting gracefully. Goodbye!")