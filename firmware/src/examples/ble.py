import asyncio
import json
import os

import aiohttp
from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

DEVICE_NAME = "ESP_MESH_BRIDGE"
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

ble_client = None

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
            elif topic.startswith("api/"):
                loop = asyncio.get_running_loop()
                loop.create_task(handle_api_topic(topic, payload))
            else:
                print(f"[DATA] Topic: {topic} | Payload: {payload}")
                
        except ValueError:
            print(f"[MALFORMED] {line}")
    else:
        print(f"[RAW] {line}")


async def handle_api_topic(topic, payload):
    global ble_client
    print(f"[API] Received topic={topic} payload={payload}")
    route_map = {
        "api/keep_alive": ("POST", "/api/keep_alive"),
        "api/get_table": ("POST", "/api/get_table"),
        "api/overview": ("GET", "/api/overview"),
        "api/stations": ("GET", "/api/stations"),
        "api/space": ("POST", "/api/space"),
        "api/create_station": ("POST", "/api/create_station"),
        "api/create_space": ("POST", "/api/create_space"),
        "api/delete_space": ("POST", "/api/delete_space"),
        "api/lock": ("POST", "/api/lock"),
        "api/unlock": ("POST", "/api/unlock"),
    }

    route = route_map.get(topic)
    if not route:
        print(f"[API] No handler for topic: {topic}")
        return

    method, path = route
    data = parse_payload(payload)
    status, response_text = await send_api_request(method, path, data)

    if ble_client is None or not ble_client.is_connected:
        print("[API] BLE client unavailable; cannot publish return topic")
        return

    endpoint = topic.split("/", 1)[1] if "/" in topic else topic
    return_topic = f"api/return/{endpoint}"
    response_payload = {"status": status, "body": response_text}
    await publish_ble_topic(ble_client, return_topic, response_payload)


API_HOSTNAME = os.getenv("API_HOSTNAME", "https://iot.corebyte.ee")
API_TOKEN = os.getenv("TOKEN", os.getenv("API_TOKEN", ""))


def default_headers():
    headers = {"Content-Type": "application/json"}
    if API_TOKEN:
        headers["Authorization"] = API_TOKEN
    return headers


def parse_payload(payload):
    if payload is None:
        return {}
    try:
        return json.loads(payload)
    except Exception:
        return payload


def build_request_body(data):
    if isinstance(data, dict):
        return data
    if isinstance(data, str) and data:
        try:
            return json.loads(data)
        except Exception:
            return {"payload": data}
    return {}


async def send_api_request(method, path, data=None):
    url = f"{API_HOSTNAME.rstrip('/')}/{path.lstrip('/')}"
    json_body = build_request_body(data)
    headers = default_headers()

    try:
        async with aiohttp.ClientSession() as session:
            if method == "GET":
                async with session.get(url, params=json_body if isinstance(json_body, dict) else None, headers=headers) as resp:
                    text = await resp.text()
                    print(f"[API] {method} {url} -> {resp.status}: {text}")
                    return resp.status, text
            else:
                async with session.post(url, json=json_body, headers=headers) as resp:
                    text = await resp.text()
                    print(f"[API] {method} {url} -> {resp.status}: {text}")
                    return resp.status, text
    except Exception as e:
        print(f"[API] Request failed for {method} {url}: {e}")
        return None, str(e)


async def publish_ble_topic(client, topic, payload):
    if client is None:
        raise ValueError("BLE client instance is required")
    if not client.is_connected:
        raise RuntimeError("BLE client is not connected")

    payload_text = payload if isinstance(payload, str) else json.dumps(payload)
    message = f"{topic}|{payload_text}\n".encode("utf-8")
    await client.write_gatt_char(CHAR_UUID, message)
    print(f"[BLE OUT] Topic: {topic} Payload: {payload_text}")


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
                global ble_client
                ble_client = client
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
                    
            ble_client = None
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