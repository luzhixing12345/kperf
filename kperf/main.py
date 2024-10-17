import argparse

from .proc import *
from .draw import draw
from .system import system_monitor



def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--pid", type=int, default=0, help="pid")
    parser.add_argument("-g", "--gid", type=int, default=0, help="gid")
    parser.add_argument("-t", "--time", type=float, default=0.1, help="sleep time")
    parser.add_argument("-f", "--file", type=str, help="load data from json file and draw")
    parser.add_argument("-o", "--output-dir", type=str, help="output dir", default="results")
    parser.add_argument("program", type=str, nargs="?", help="program name")
    args = parser.parse_args()

    if args.file:
        with open(args.file, "r") as f:
            data = json.load(f)
        draw(data)
        return

    # check if is in linux system
    if not sys.platform.startswith("linux"):
        print("only support linux system")
        return
    get_sudo()
    if args.pid > 0:
        proc = Proc(args.pid)
        proc.dump()
    elif args.gid > 0:
        proc = Proc(args.gid)
        proc.dump()
    elif args.program:
        print(f"run {args.program}")
        try:
            proc = Proc(run(args.program))
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
    draw(proc.statistic_infos, args.output_dir)


if __name__ == "__main__":
    main()
