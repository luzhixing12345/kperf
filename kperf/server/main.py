import argparse
import os
import shutil
from pathlib import Path
from .config import *
from . import config
from .server import start_http_server, set_start_time
from .libbpf import *

def load_bpf_program():
    # bpf_program = BPFProgram()
    # bpf_program.load_program()
    # bpf_program.attach_kprobe(event="sys_execve", fn_name="sys_execve_entry")
    # bpf_program.attach_kretprobe(event="sys_execve", fn_name="sys_execve_exit")
    ...


def run_profiler():
    # create dir
    if not os.path.exists(RESULT_DIR):
        os.makedirs(RESULT_DIR)

    # copy each asset file to result dir
    assets_path = Path(__file__).parent.parent / "assets"
    for asset in assets_path.iterdir():
        if asset.is_file():
            if asset.name == "index.html":
                content = asset.read_text()
                ws_code = """
                <script>
                    (function(){
                        const protocol = (location.protocol === 'https:') ? 'wss' : 'ws';
                        const host = location.hostname;
                        const port = WS_PORT;
                        const ws = new WebSocket(protocol + '://' + host + ':' + port);
                        ws.onmessage = (event) => {
                            if (event.data === "reload") location.reload();
                        };
                        ws.onclose = () => { console.warn('WebSocket closed'); };
                        ws.onerror = (e) => { console.warn('WebSocket error', e); };
                    })();
                </script>
                    """
                ws_code = ws_code.replace("WS_PORT", str(config.WEBSOCKET_PORT))
                content = content.replace("$websocket-code", ws_code)
                with open(RESULT_DIR + "/index.html", "w") as f:
                    f.write(content)
            else:
                shutil.copy(asset, RESULT_DIR)


def main():
    parser = argparse.ArgumentParser(description="Kperf Server")
    parser.add_argument("--port", type=int, default=PORT, help="Port to run the server on")
    parser.add_argument("--generate", "-g", action="store_true", help="Only generate the result files")
    args = parser.parse_args()

    set_start_time()

    load_bpf_program()
    run_profiler()

    # Start the server
    if not args.generate:
        start_http_server(port=args.port)


if __name__ == "__main__":
    main()
