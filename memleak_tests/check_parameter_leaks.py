#!/usr/bin/env python3

from __future__ import annotations
import gc

import pycsh
import resource
from time import time
from sys import getrefcount

def check_PythonParameter_leaks(iterations=10000):

    start_mem_usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    # param_name: str = pycsh.list()[0]
    for i in range(iterations):
    # i = 0
    # while True:
    #     i = i+1
        current_mem_usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss - start_mem_usage
        # print(f"{i=}\t{current_mem_usage=}")

        def memleak_test_callback(param: pycsh.PythonParameter, offset: int):
            pass

        try:
            # TODO Kevin: 'param' does not appear to be garbage collected while the loop is running.
            param = pycsh.PythonParameter(600, "memleak_test_param", pycsh.PARAM_TYPE_INT8, pycsh.PM_DEBUG, "test", "memleak test param", 8)
            print(f"type {type(param)}")
            print(f"refcountA {getrefcount(param)}")
            param.keep_alive = False
            print(f"refcountB {getrefcount(param)}")
            gc.collect()
            print("Created")
        except ValueError:  # Parameter with ID already exists (hasn't been garbage collected yet)
            # print("Skip")
            pass


def check_Parameter_leaks(iterations=10000):

    start_mem_usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    param_name: str = pycsh.list()[0]
    for i in range(iterations):
    # i = 0
    # while True:
    #     i = i+1
        current_mem_usage = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss - start_mem_usage
        print(f"{i=}\t{current_mem_usage=}")

        param = pycsh.Parameter(param_name)
        #print(f"refcount {getrefcount(param)}")


def main() -> None:


    pycsh.init(quiet=False)
    pycsh.csp_init()

    # pycsh.PythonParameter(600, "memleak_test_param", pycsh.PARAM_TYPE_INT8, pycsh.PM_DEBUG, "test", "memleak test param", 0, None).keep_alive = False

    #check_PythonParameter_leaks()
    check_Parameter_leaks()

    print("New param")
    param = pycsh.PythonParameter(600, "memleak_test_param", pycsh.PARAM_TYPE_INT8, pycsh.PM_DEBUG, "test", "memleak test param", 8)


if __name__ == '__main__':
    main()