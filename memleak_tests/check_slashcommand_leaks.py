#!/usr/bin/env python3

from __future__ import annotations

import pycsh
import resource
from time import time

# def check_memory_leaks(sec_duration=10):

#     start_timestamp = time()
#     while time() < start_timestamp + sec_duration:
#         pass


def check_PythonSlashCommand_leaks(iterations=10000):

    start_mem_usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    for i in range(iterations):
    # while True:
    #     i = 0
        current_mem_usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss - start_mem_usage
        print(f"{i=}\t{current_mem_usage=}")

        def command_with_docstr() -> None:
            """
            Test memory leak with docstring
            """
            return
        pycsh.PythonSlashCommand("command_with_docstr", command_with_docstr)

        def command_without_docstr() -> None:
            return
        pycsh.PythonSlashCommand("command_without_docstr", command_without_docstr).keep_alive = True

        def command_with_argstr() -> None:
            return
        pycsh.PythonSlashCommand("command_with_argstr", command_with_argstr, "argstring goes here")

        def command_with_all() -> None:
            """ Has both docstr and argstr """
            return
        pycsh.PythonSlashCommand("command_with_all", command_with_all, "argstring goes here")


def main() -> None:


    pycsh.init(quiet=True)
    pycsh.csp_init()

    check_PythonSlashCommand_leaks()


if __name__ == '__main__':
    main()