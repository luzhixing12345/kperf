import os
import socket
import threading
from http.server import HTTPServer, SimpleHTTPRequestHandler
import time
import webbrowser
import errno
import sys
import time
from .data_transport import find_available_ws_port, start_ws_server
from .config import *

start_time = 0
ARROW_CHAR = "➜  "
DEBOUNCE_INTERVAL = 2.0  # 防抖时间（秒）


def set_start_time():
    global start_time
    start_time = time.time()


def is_wsl():
    try:
        with open("/proc/version", "r") as f:
            version_info = f.read()
            return "microsoft-standard-WSL" in version_info
    except FileNotFoundError:
        # 如果没有该文件,说明不是 WSL 环境
        return False

class SilentHTTPRequestHandler(SimpleHTTPRequestHandler):
    def log_message(self, format, *args):
        # 重写这个方法来抑制日志输出
        pass

    def do_GET(self):
        try:
            # 这里放你发送数据的代码,例如发送文件或响应
            super().do_GET()  # 调用父类的 GET 处理方法
        except BrokenPipeError:
            # 捕获并处理 BrokenPipeError
            # print("Client disconnected before sending the response.")
            pass
        except ConnectionResetError:
            pass
        except Exception as e:
            # 捕获其他异常,避免服务器崩溃
            print(f"An error occurred: {e}")


def check_port_available(port):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.bind(("localhost", port))
            return True
        except socket.error as e:
            return False
        except Exception as e:
            return False


def find_available_port(start_port=8000, end_port=9000):
    # 创建一个socket对象
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    for port in range(start_port, end_port + 1):
        try:
            sock.bind(("", port))
            break
        except OSError:
            continue
    # 获取绑定的地址和端口
    host, port = sock.getsockname()
    # 关闭socket
    sock.close()
    return port


def get_ip_address():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        # doesn't even have to be reachable
        s.connect(("10.255.255.255", 1))
        IP = s.getsockname()[0]
    except Exception:
        IP = "127.0.0.1"
    finally:
        s.close()
    return IP


def info(msg, color=None, end=""):
    if color == "red":
        print(f"\033[1;31m{msg}\033[0m", end=end)
    elif color == "green":
        print(f"\033[1;32m{msg}\033[0m", end=end)
    elif color == "grey":
        print(f"\033[1;30m{msg}\033[0m", end=end)
    elif color == "blue":
        print(f"\033[1;34m{msg}\033[0m", end=end)
    elif color == "cyan":
        print(f"\033[1;36m{msg}\033[0m", end=end)
    elif color == "strong":
        print(f"\033[1m{msg}\033[0m", end=end)
    else:
        print(msg, end=end)


def show_server_info(time, port: int, result_dir):
    info("\n    ")
    info(f"Kperf v{version_str()}", "green")
    info(f"  ready in {time} ms")
    info("\n\n    ")
    info(ARROW_CHAR, "green")
    info("Local:   ", "strong")
    info(f"http://127.0.0.1:{port}/{result_dir}/index.html", "blue")
    info("\n    ")
    info(ARROW_CHAR, "green")
    info("Remote:  ", "strong")
    ip = get_ip_address()
    info(f"http://{ip}:{port}/{result_dir}/index.html", "blue")
    info("\n    ")
    info(ARROW_CHAR, "green")
    info("press ")
    info("q / CTRL+C", "strong")
    info(" to quit")
    info("\n\n    ")


def clear_screen():
    os.system("cls" if os.name == "nt" else "clear")


def start_http_server(result_dir: str = RESULT_DIR, port: int = PORT):
    # 切换到指定的目录
    directory = os.path.join(os.getcwd(), result_dir)
    if port is None or not check_port_available(port):
        port = find_available_port()
    else:
        # check if port is available
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            try:
                s.bind(("localhost", port))
            except socket.error as e:
                if e.errno == errno.EADDRINUSE:
                    print(f"Port {port} is already in use. Please choose another port.")
                    return
                else:
                    raise
                
    find_available_ws_port()
    start_ws_server_thread = threading.Thread(target=start_ws_server)
    start_ws_server_thread.daemon = True
    start_ws_server_thread.start()

    # 创建 HTTP 服务器
    with HTTPServer(("", port), SilentHTTPRequestHandler) as httpd:
        port = httpd.server_address[1]
        httpd.RequestHandlerClass.directory = directory
        # 创建一个线程来运行服务器
        server_thread = threading.Thread(target=httpd.serve_forever)
        server_thread.daemon = True
        server_thread.start()
        end_time = time.time()

        url = f"http://127.0.0.1:{port}/{result_dir}/index.html"
        if not is_wsl():
            webbrowser.open(url)
        else:
            os.system(f'cmd.exe /c start "" "{url}"')

        try:
            while True:
                # clear_screen()
                show_server_info((int((end_time - start_time) * 1000)), port, result_dir)
                command = input()
                if command.lower() == "q":
                    break
        except KeyboardInterrupt:
            print("\nServer is shutting down...")
        finally:
            httpd.shutdown()  # 停止服务器
            httpd.server_close()  # 关闭服务器socket
            server_thread.join(timeout=1)  # 等待服务器线程结束,设置超时时间为1秒
            sys.exit(0)  # 使用sys.exit(0)正常退出
