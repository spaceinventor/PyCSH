#!/usr/bin/env python3

import os
import sys
import unittest
import subprocess
from os.path import dirname,  realpath
from multiprocessing import Pool, cpu_count

# Ensure project root is in sys.path so 'tests' package can be imported
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Custom exit code we decide for Valgrind errors
VALGRIND_EXITCODE: int = 99

VALGRIND = [
    "valgrind",
    '-s',
    "--quiet",
    "--exit-on-first-error=yes",  # Each test gets a separate process, so we can check which exact ones give memory errors.
    f"--error-exitcode={VALGRIND_EXITCODE}",
]

MAX_WORKERS = max(1, cpu_count() // 2)


def run_test(test_id):
    import test_parameter
    """Run a single unittest under Valgrind."""
    cmd = VALGRIND + [sys.executable, "-m", "unittest", test_id]
    proc = subprocess.run(cmd, capture_output=True, text=True)

    if proc.returncode == 0:
        return (test_id, "ok", proc.stdout, proc.stderr)
    elif proc.returncode == VALGRIND_EXITCODE:
        return (test_id, "valgrind", proc.stdout, proc.stderr)
    else:
        return (test_id, "fail", proc.stdout, proc.stderr)


def iter_test_ids(suite):
    """Recursively yield test IDs from a TestSuite/TestCase tree."""
    if isinstance(suite, unittest.TestSuite):
        for t in suite:
            yield from iter_test_ids(t)
    else:
        yield suite.id()



class ValgrindTestRunner:
    def __init__(self, verbosity=1, max_workers=MAX_WORKERS):
        self.verbosity = verbosity
        self.max_workers = max_workers
        self.results = []

    def run(self, suite):
        # Flatten all test cases into test IDs
        test_ids = list(iter_test_ids(suite))

        if self.verbosity > 0:
            print(f"Running {len(test_ids)} tests under Valgrind "
                  f"using {self.max_workers} workers...")

        with Pool(processes=self.max_workers) as pool:
            self.results = pool.map(run_test, test_ids)

        # Report results
        failures = []
        for test_id, status, out, err in self.results:
            if status == "ok":
                if self.verbosity > 1:
                    print(f"✔ {test_id}")
            elif status == "valgrind":
                print(f"❌ {test_id} (Valgrind error)")
                if err.strip():
                    print(err.strip())
                failures.append((test_id, "Valgrind error"))
            else:
                print(f"❌ {test_id} (Test failure)")
                if out.strip():
                    print(out.strip())
                if err.strip():
                    print(err.strip())
                failures.append((test_id, "Test failure"))

        print("\n=== TEST SUMMARY ===")
        if not failures:
            print("All tests passed under Valgrind ✅")
            return True
        else:
            for test_id, reason in failures:
                print(f"❌ {test_id} failed: {reason}")
            return False


if __name__ == "__main__":
    # Discover tests recursively (e.g. tests/test_*.py)
    os.chdir(dirname(realpath(__file__)))  # Fix `unittest` working directory, so imports work
    suite = unittest.defaultTestLoader.discover(os.path.dirname(os.path.abspath(__file__)))

    runner = ValgrindTestRunner(verbosity=1)
    success = runner.run(suite)
    sys.exit(0 if success else 1)
