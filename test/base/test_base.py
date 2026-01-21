import os
import sys
import time
import unittest
import subprocess
from pathlib import Path
import re
import signal


class KperfBaseTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workspace = Path(__file__).resolve().parents[2]
        cls.kperf_bin = cls.workspace / "build" / "kperf"
        cls.test_dir = cls.workspace / "test" / "base"

        # require root to run perf; skip if not root to avoid failing CI where root not available
        if os.geteuid() != 0:
            sys.exit("Requires root to run perf-based integration test (run as root or use sudo)")

    def run_kperf(self, test_program, extra_args=None):
        cmd = [str(self.kperf_bin), "-d", "--tui", "--", str(self.test_dir / test_program)]
        if extra_args:
            cmd.extend(extra_args)
        # Run kperf and wait for it to finish; use timeout to avoid hanging tests
        try:
            proc = subprocess.run(
                cmd,
                timeout=30,
                check=False,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
        except subprocess.TimeoutExpired:
            self.fail("kperf did not finish within timeout")
        return proc.stdout, proc.stderr

    def parse_kperf_tui_output(self, stdout: str):
        pattern = re.compile(r"([\w\.@]+) \((\d+\.\d+)%")
        matches = pattern.findall(stdout)
        profiled_dict = {}
        for match in matches:
            profiled_dict[match[0]] = float(match[1])
        return profiled_dict

    def run_kperf_until_signal(self, test_program, sigs=[signal.SIGINT], run_seconds=2, extra_args=None):
        cmd = [str(self.kperf_bin), "-d", "--tui", "--", str(self.test_dir / test_program)]
        if extra_args:
            cmd.extend(extra_args)

        proc = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        line = proc.stdout.readline()  # type: ignore
        # print(line)
        # [DEBUG][src/kperf/main.c:147(main)] child pid = 572295
        child_pid = int(line.split()[-1])
        # print(f"child pid = {child_pid}")

        try:
            time.sleep(run_seconds)
            for sig in sigs:
                os.kill(child_pid, sig)  # only send signal to child process
                time.sleep(0.5)
            stdout, stderr = proc.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            stdout, stderr = proc.communicate()
            self.fail("kperf did not terminate after SIGINT")

        return proc.returncode, stdout, stderr

    def basic_output_check(self, stdout: str):
        self.assertIn("/usr/lib/x86_64-linux-gnu/libc.so", stdout)
        self.assertIn("/usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so", stdout)
        # load user symbol 2778 items
        reg_pattern = r"load user symbol (\d+) items"
        match = re.search(reg_pattern, stdout)
        self.assertIsNotNone(match)
        self.assertGreater(int(match.group(1)), 2000)  # type: ignore

    def test_simple(self):
        stdout, stderr = self.run_kperf("simple")

        # basic checks
        print(stdout)
        self.assertEqual(stderr, "")

        # print(stdout)
        self.assertIn("Hello World", stdout)
        self.basic_output_check(stdout)

    def test_signal(self):
        stdout, stderr = self.run_kperf("signal")
        print(stdout)
        self.assertEqual(stderr, "")
        self.assertIn("tracee stopped by signal 2", stdout)  # SIGINT
        self.assertIn("tracee stopped by signal 10", stdout)
        self.assertIn("tracee about to exit, code=0", stdout)

        # profile_output = self.parse_kperf_tui_output(stdout)
        # self.assertEqual(profile_output["main"], 100)

        self.basic_output_check(stdout)

    def test_func_call(self):
        stdout, stderr = self.run_kperf("func_call")

        # basic checks
        self.parse_kperf_tui_output(stdout)
        # print(stdout)
        # === Results ===
        # Execution time (seconds):
        # foo1: 0.780814 (18.9%) - 1000000000 iterations
        # foo2: 1.339246 (32.4%) - 2000000000 iterations
        # foo3: 2.008622 (48.7%) - 3000000000 iterations
        # Total: 4.128681 seconds
        # [DEBUG][src/kperf/symbol.c:21(load_elf_symbol)] load elf file: /home/lzx/kperf/test/base/func_call
        # [DEBUG][src/kperf/symbol.c:21(load_elf_symbol)] load elf file: /usr/lib/x86_64-linux-gnu/libc.so.6
        # [DEBUG][src/kperf/symbol.c:21(load_elf_symbol)] load elf file: /usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2
        # [DEBUG][src/kperf/symbol.c:281(load_user_symbols)] load user symbol 2782 items
        # [DEBUG][src/kperf/main.c:165(main)] tracee about to exit, code=0
        # [INFO][src/kperf/main.c:178(main)] perf sample size = 3200
        # └── __libc_init_first (100.0% 3200/3200)
        #     └── main (100.0% 3200/3200)
        #         ├── foo3 (48.5% 1553/3200)
        #         ├── foo2 (32.5% 1040/3200)
        #         └── foo1 (19.0% 607/3200)
        self.basic_output_check(stdout)

        # parse foo1/2/3 percentages from the Execution time section
        # expected percents (approx)
        program_output_pat = re.compile(r"(?m)^\s*(foo[123])\s*:\s*[0-9]+(?:\.[0-9]+)?\s*\(\s*([0-9.]+)%\s*\)")
        found = {}
        for m in program_output_pat.finditer(stdout):
            found[m.group(1)] = float(m.group(2))

        # print(found)
        # prefer numerical Execution time parsing; fall back to tree percentages if missing
        if len(found) < 3:
            # tree-style lines like: "├── foo3 (48.5% 1553/3200)"
            for m in re.finditer(r"(?m)^(?:.*)(foo[123])\s*\(([0-9.]+)%", stdout):
                found[m.group(1)] = float(m.group(2))

        # foo3 (48.5% 1553/3200)
        kperf_output_pat = re.compile(r"\s*(foo[123])\s* \(\s*([0-9.]+)%")
        kperf_found = {}
        for m in kperf_output_pat.finditer(stdout):
            kperf_found[m.group(1)] = float(m.group(2))
        # print(kperf_found)

        self.assertEqual(len(found), 3)
        self.assertEqual(len(kperf_found), 3)

        # compare with tolerance 1.0%
        for k, ev in kperf_found.items():
            av = found[k]
            diff = abs(av - ev)
            self.assertLessEqual(diff, 1.0, f"{k} percent mismatch: actual={av} expected={ev} diff={diff}")
