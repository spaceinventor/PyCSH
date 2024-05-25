#!/usr/bin/env python3

from __future__ import annotations

from subprocess import PIPE, run


def main() -> None:
    ersion: str = run(['git', 'describe', '--long', '--always', '--dirty=+'], stdout=PIPE).stdout.decode()
    ersion = ersion.lstrip('vV')

    semantic_ersion = ersion.split('-')[0]

    num_commits_ahead = int(ersion.split('-')[1])

    if num_commits_ahead > 0:
        semantic_ersion += f".post{num_commits_ahead}"

    if ersion.endswith('+'):  # Dirty working tree
        semantic_ersion += '.dev0'

    print(semantic_ersion, end='')

if __name__ == '__main__':
    main()
