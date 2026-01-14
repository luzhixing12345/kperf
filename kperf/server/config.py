RESULT_DIR = "kperf-result"
VERSION = (0, 0, 1)
PORT = 36002
WEBSOCKET_PORT = 8765

def version_str():
    return ".".join(map(str, VERSION))