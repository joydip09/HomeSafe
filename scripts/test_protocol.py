#!/usr/bin/env python3
"""Black-box WebSocket protocol checks for a running HomeSafe ESP32."""

import argparse
import asyncio
import json
import os
import sys
import time
from typing import Any

import websockets


EVENT_TYPES = {
    "acoustic_event",
    "hazard_event",
    "security_event",
    "alarm_event",
}
STATE_SECTIONS = {
    "device",
    "environment",
    "hazards",
    "audio",
    "security",
    "alarms",
}


class ProtocolFailure(RuntimeError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ProtocolFailure(message)


def parse_message(raw: Any) -> dict[str, Any]:
    require(isinstance(raw, str), "WebSocket frame was not text")
    try:
        message = json.loads(raw)
    except json.JSONDecodeError as error:
        raise ProtocolFailure(f"received invalid JSON: {error}") from error
    require(isinstance(message, dict), "received JSON message was not an object")
    require(message.get("version") == 1, "message has the wrong protocol version")
    require(isinstance(message.get("type"), str), "message has no type")
    require(isinstance(message.get("timestamp_ms"), int), "message has no timestamp")
    require(isinstance(message.get("sequence"), int), "message has no sequence")
    return message


def validate_message(message: dict[str, Any]) -> None:
    message_type = message["type"]
    if message_type in {"state_snapshot", "state_update"}:
        require(isinstance(message.get("data"), dict), f"{message_type} has no data")
        if message_type == "state_snapshot":
            require(set(message["data"]) == STATE_SECTIONS,
                    "snapshot does not contain exactly the state sections")
        else:
            require(set(message["data"]).issubset(STATE_SECTIONS),
                    "state update contains an unknown section")
            require(bool(message["data"]), "state update contains no sections")
    elif message_type == "command_result":
        require(isinstance(message.get("request_id"), str),
                "command result has no request_id")
        require(isinstance(message.get("command"), str),
                "command result has no command")
        require(isinstance(message.get("success"), bool),
                "command result has no success flag")
        if not message["success"]:
            error = message.get("error")
            require(isinstance(error, dict), "failed command has no error object")
            require(isinstance(error.get("code"), str), "error has no code")
            require(isinstance(error.get("message"), str), "error has no message")
    elif message_type in EVENT_TYPES:
        require(isinstance(message.get("event_id"), int),
                f"{message_type} has no event_id")
        require(isinstance(message.get("data"), dict),
                f"{message_type} has no data")
        if message_type == "acoustic_event":
            require(isinstance(message["data"].get("detected_class"), str),
                    "acoustic event has no detected_class")
            require(isinstance(message["data"].get("confidence"), (int, float)),
                    "acoustic event has no confidence")
        elif message_type == "hazard_event":
            require(isinstance(message["data"].get("source"), str),
                    "hazard event has no source")
            require(isinstance(message["data"].get("active"), bool),
                    "hazard event has no active state")
            require(isinstance(message["data"].get("environmental_active"), bool),
                    "hazard event has no environmental state")
        elif message_type == "security_event":
            require(isinstance(message["data"].get("name"), str),
                    "security event has no name")
        else:
            require(isinstance(message["data"].get("source"), str),
                    "alarm event has no source")
            require(isinstance(message["data"].get("action"), str),
                    "alarm event has no action")
    else:
        raise ProtocolFailure(f"unknown received message type: {message_type}")


class Client:
    def __init__(self, websocket: Any, messages: list[dict[str, Any]]) -> None:
        self.websocket = websocket
        self.messages = messages

    async def receive(self, timeout: float) -> dict[str, Any]:
        raw = await asyncio.wait_for(self.websocket.recv(), timeout)
        message = parse_message(raw)
        validate_message(message)
        self.messages.append(message)
        return message

    async def command(self, name: str, request_id: str, data: dict[str, Any] | None = None) -> dict[str, Any]:
        request = {
            "version": 1,
            "type": "command",
            "command": name,
            "request_id": request_id,
        }
        if data is not None:
            request["data"] = data
        await self.websocket.send(json.dumps(request))
        deadline = time.monotonic() + self.timeout
        while True:
            remaining = deadline - time.monotonic()
            require(remaining > 0, f"timed out waiting for response to {request_id}")
            message = await self.receive(remaining)
            if (message["type"] == "command_result" and
                    message.get("request_id") == request_id):
                return message

    @property
    def timeout(self) -> float:
        return self._timeout


async def wait_for_snapshot(client: Client, timeout: float) -> dict[str, Any]:
    deadline = time.monotonic() + timeout
    while True:
        remaining = deadline - time.monotonic()
        require(remaining > 0, "timed out waiting for state_snapshot")
        message = await client.receive(remaining)
        if message["type"] == "state_snapshot":
            return message


async def run(args: argparse.Namespace) -> None:
    uri = f"ws://{args.host}:{args.port}"
    messages: list[dict[str, Any]] = []
    async with websockets.connect(uri, open_timeout=args.timeout) as socket:
        client = Client(socket, messages)
        client._timeout = args.timeout
        snapshot = await wait_for_snapshot(client, args.timeout)
        require(set(snapshot["data"]) == STATE_SECTIONS,
                "initial snapshot is incomplete")

        status = await client.command("get_status", "stage8-status", {})
        require(status["success"] is True, "get_status was not successful")
        require(set(status["data"]["state"]) == STATE_SECTIONS,
                "get_status state is incomplete")
        initial_armed = status["data"]["state"]["security"]["armed"]

        invalid_cases = [
            ("{not-json", "invalid_json"),
            (json.dumps({"version": 1, "type": "state_snapshot"}), "invalid_message_type"),
            (json.dumps({"version": 99, "type": "command"}), "unsupported_version"),
            (json.dumps({"version": 1, "type": "command", "command": "get_status"}),
             "missing_request_id"),
            (json.dumps({"version": 1, "type": "command", "request_id": "bad",
                         "command": "not_a_command"}), "unknown_command"),
            (json.dumps({"version": 1, "type": "command", "request_id": "bad-type",
                         "command": "set_security_mode", "data": {"armed": "yes"}}),
             "missing_or_invalid_armed"),
            (json.dumps({"version": 1, "type": "command", "request_id": "bad-time",
                         "command": "set_security_schedule",
                         "data": {"enabled": True, "arm_hour": 25,
                                  "arm_minute": 0, "disarm_hour": 6,
                                  "disarm_minute": 0}}), "invalid_schedule"),
        ]
        for index, (payload, expected_code) in enumerate(invalid_cases):
            await socket.send(payload)
            deadline = time.monotonic() + args.timeout
            response = None
            while time.monotonic() < deadline:
                candidate = await client.receive(
                    max(0.01, deadline - time.monotonic())
                )
                if candidate["type"] == "command_result":
                    response = candidate
                    break
            require(response is not None,
                    f"invalid case {index} did not return command_result")
            require(response.get("success") is False,
                    f"invalid case {index} was accepted")
            require(response["error"]["code"] == expected_code,
                    f"invalid case {index}: expected {expected_code}, got "
                    f"{response['error']['code']}")

        for command, data in [
            ("set_security_mode", {"armed": not initial_armed}),
            ("set_security_mode", {"armed": initial_armed}),
            ("set_security_schedule", {"enabled": False}),
            ("silence_alarm", {}),
        ]:
            response = await client.command(command, f"stage8-{command}", data)
            require(response["success"] is True,
                    f"{command} did not report applied success")

        update_messages = [
            message for message in messages if message["type"] == "state_update"
        ]
        security_updates = 0
        for update in update_messages:
            require(set(update["data"]).issubset(STATE_SECTIONS),
                    "state update contains an unknown top-level section")
            if "security" in update["data"]:
                security_updates += 1
        require(security_updates > 0,
                "security mode transition did not produce a security update")

        if args.clients:
            second_messages: list[dict[str, Any]] = []
            async with websockets.connect(uri, open_timeout=args.timeout) as second_socket:
                second = Client(second_socket, second_messages)
                second._timeout = args.timeout
                await wait_for_snapshot(second, args.timeout)
                await second.command("get_status", "stage8-second-status", {})
            require(any(m["type"] == "state_snapshot" for m in second_messages),
                    "second client did not receive a snapshot")

        sequences = [message["sequence"] for message in messages]
        require(sequences == sorted(set(sequences)),
                "received outbound sequences are not strictly increasing")
        event_ids = [message["event_id"] for message in messages
                     if message["type"] in EVENT_TYPES]
        require(len(event_ids) == len(set(event_ids)),
                "received event IDs are not unique")

    async with websockets.connect(uri, open_timeout=args.timeout) as reconnect_socket:
        reconnect_messages: list[dict[str, Any]] = []
        reconnect = Client(reconnect_socket, reconnect_messages)
        reconnect._timeout = args.timeout
        await wait_for_snapshot(reconnect, args.timeout)
        require(reconnect_messages[0]["type"] == "state_snapshot",
                "reconnect did not begin with a snapshot")

    received_events = {message["type"] for message in messages} & EVENT_TYPES
    missing_events = EVENT_TYPES - received_events
    if missing_events:
        print("NOTE: event types not observed in this run:", ", ".join(sorted(missing_events)))
        print("      Trigger the corresponding physical sensor/state transition to test them.")

    print(f"PASS: validated {len(messages)} received messages from {uri}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=os.getenv("HOMESAFE_HOST", "192.168.0.111"))
    parser.add_argument("--port", type=int,
                        default=int(os.getenv("HOMESAFE_PORT", "81")))
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--clients", action="store_true",
                        help="also test a simultaneous second client")
    args = parser.parse_args()
    try:
        asyncio.run(run(args))
    except (OSError, websockets.WebSocketException, asyncio.TimeoutError) as error:
        print(f"SKIP/FAIL: could not complete hardware integration test: {error}",
              file=sys.stderr)
        return 2
    except ProtocolFailure as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
