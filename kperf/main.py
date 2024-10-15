import argparse
import subprocess
from .proc import *
from .draw import draw
from .system import system_monitor


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--pid", type=int, default=0, help="pid")
    parser.add_argument("-g", "--gid", type=int, default=0, help="gid")
    parser.add_argument("-t", "--time", type=float, default=0.1, help="sleep time")
    parser.add_argument("-f", "--file", type=str, help="load data from json file and draw")
    parser.add_argument("program", type=str, nargs="?", help="program name")
    args = parser.parse_args()

    if args.file:
        with open(args.file, "r") as f:
            data = json.load(f)
        draw(data)
        return

    if args.pid > 0:
        proc = Proc(args.pid)
        proc.dump()
    elif args.gid > 0:
        proc = Proc(args.gid)
        proc.dump()
    elif args.program:
        print(f"run {args.program}")
        try:
            process = subprocess.Popen(args.program.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            proc = Proc(process.pid)
        except FileNotFoundError:
            print(f"could not find program {args.program}")
            return
        except OSError as e:
            print(f"error when running: {e}")
            return
    else:
        # if no pid or gid or program, monitor system data
        return system_monitor()

    proc.sleep_time = args.time
    proc.monitor()
    proc.save_data()
    draw(proc.mem_status_list)


if __name__ == "__main__":
    main()