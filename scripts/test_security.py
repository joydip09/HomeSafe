import asyncio
import json
import websockets

ESP32_IP = "10.39.236.173"
PORT = 81

async def main():
    uri = f"ws://{ESP32_IP}:{PORT}"

    async with websockets.connect(uri) as ws:
        command = {
            "version": 1,
            "type": "command",
            "command": "set_security_mode",
            "request_id": "arm-001",
            "data": {
                "armed": True
            }
        }

        await ws.send(json.dumps(command))
        response = json.loads(await ws.recv())

        print("Arm response:")
        print(json.dumps(response, indent=2))

        # Query status to verify the result.
        status_command = {
            "version": 1,
            "type": "command",
            "command": "get_status",
            "request_id": "status-001"
        }

        await ws.send(json.dumps(status_command))
        status = json.loads(await ws.recv())

        print("\nCurrent HomeSafe status:")
        print(json.dumps(status, indent=2))

asyncio.run(main())