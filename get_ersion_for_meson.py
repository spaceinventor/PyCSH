#!/usr/bin/env python3

from __future__ import annotations

from subprocess import PIPE, run


def main() -> None:
    ersion: str = run(['git', 'describe', '--long', '--always', '--dirty=+'], stdout=PIPE).stdout.decode()
    ersion = ersion.lstrip('vV')
    lst = ersion.split('.')
    if len(lst) <= 2:  # Not semantic versioning
        ersion = ersion.replace('-', '.', 1)

    ersion = ersion.split('-')[0]

    print(ersion, end='')

if __name__ == '__main__':
    main()
