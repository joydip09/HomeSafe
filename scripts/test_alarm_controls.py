import asyncio
import json
import websockets

ESP32_IP = "192.168.10.207"  # Update if the ESP32 IP changes
PORT = 81


async def send_command(ws, command, request_id, data=None):
    message = {
        "version": 1,
        "type": "command",
        "command": command,
        "request_id": request_id,
    }

    if data is not None:
        message["data"] = data

    await ws.send(json.dumps(message))
    response = json.loads(await ws.recv())

    print(f"\n{command}:")
    print(json.dumps(response, indent=2))
    return response


async def main():
    uri = f"ws://{ESP32_IP}:{PORT}"

    async with websockets.connect(uri) as ws:
        # 1. Check the current state.
        await send_command(ws, "get_status", "status-before")

        # 2. Silence the security alarm.
        await send_command(ws, "silence_alarm", "silence-001")

        # 3. Verify the resulting state.
        await send_command(ws, "get_status", "status-after-silence")

        # 4. Disarm security.
        await send_command(
            ws,
            "set_security_mode",
            "disarm-001",
            {"armed": False},
        )

        # 5. Verify the final state.
        await send_command(ws, "get_status", "status-final")


asyncio.run(main())