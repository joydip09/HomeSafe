import asyncio
import websockets

ESP32_IP = "10.39.236.173"
URI = f"ws://{ESP32_IP}:81"

async def main():
    print(f"Connecting to HomeSafe at {URI}...")

    try:
        async with websockets.connect(URI, open_timeout=5) as ws:
            print("Connected successfully!")

            message = (
    '{"version":1,'
    '"type":"command",'
    '"command":"get_status",'
    '"request_id":"test-001"}'
)
            await ws.send(message)
            print(f"Sent: {message}")

            try:
                response = await asyncio.wait_for(ws.recv(), timeout=3)
                print(f"Received: {response}")
            except asyncio.TimeoutError:
                print("No response received (expected at this stage).")

    except Exception as error:
        print(f"Connection failed: {error}")

asyncio.run(main())
