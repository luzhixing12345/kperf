import websockets
import asyncio
import socket
from . import config

clients = set()


async def handler(websocket, *path):
    clients.add(websocket)
    try:
        await websocket.wait_closed()
    finally:
        clients.remove(websocket)


async def notify_clients():
    if clients:
        # print("Notifying clients to reload...")
        await asyncio.gather(*[ws.send("reload") for ws in clients])


async def ws_main():
    # Bind to all interfaces so browsers on other hosts can connect.
    host = "0.0.0.0"
    async with websockets.serve(handler, host, config.WEBSOCKET_PORT):
        # print(f"WebSocket server started on ws://{host}:{config.WEBSOCKET_PORT}")
        await asyncio.Future()


def start_ws_server():
    asyncio.run(ws_main())


def find_available_ws_port():
    port = config.WEBSOCKET_PORT
    while True:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                # bind to all interfaces to test port availability
                s.bind(("0.0.0.0", port))
                config.WEBSOCKET_PORT = port
                return
            except socket.error:
                port += 1
        if port > 65535:
            raise RuntimeError("No available ports for WebSocket server.")
