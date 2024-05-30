#!/usr/bin/env python3

from __future__ import annotations
import inspect

import pycsh
import resource
from time import time


def check_PythonSlashCommand_leaks(iterations=10000):

    start_mem_usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    for i in range(iterations):
    # i = 0
    # while True:
    #     i = i+1
        current_mem_usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss - start_mem_usage
        print(f"{inspect.currentframe().f_code.co_name}\t{i=}\t{current_mem_usage=}")

        def command_with_docstr() -> None:
            """
            Test memory leak with docstring
            """
            return
        pycsh.PythonSlashCommand("command_with_docstr", command_with_docstr)

        def command_without_docstr() -> None:
            return
        pycsh.PythonSlashCommand("command_without_docstr", command_without_docstr)

        def command_with_argstr() -> None:
            return
        pycsh.PythonSlashCommand("command_with_argstr", command_with_argstr, "argstring goes here")

        def command_with_all() -> None:
            """ Has both docstr and argstr """
            return
        pycsh.PythonSlashCommand("command_with_all", command_with_all, "argstring goes here")


def check_SlashCommand_leaks(iterations=10000):

    start_mem_usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    for i in range(iterations):
    # i = 0
    # while True:
    #     i = i+1
        current_mem_usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss - start_mem_usage
        print(f"{inspect.currentframe().f_code.co_name}\t{i=}\t{current_mem_usage=}")

        pycsh.SlashCommand("help")()


def main() -> None:


    pycsh.init(quiet=True)
    pycsh.csp_init()

    check_PythonSlashCommand_leaks()
    check_SlashCommand_leaks()


if __name__ == '__main__':
    main()