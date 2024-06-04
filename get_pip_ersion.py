#!/usr/bin/env python3
""" Print the Pip compliant semantic version of the Git tag, which is without the 'v' """

from __future__ import annotations

from subprocess import PIPE, run
from datetime import datetime


def main() -> None:
    ersion: str = run(['git', 'describe', '--long', '--always', '--dirty=+'], stdout=PIPE).stdout.decode()
    ersion = ersion.lstrip('vV').strip('\n\t ')

    semantic_ersion = ersion.split('-')[0]

    num_commits_ahead = int(ersion.split('-')[1])

    if num_commits_ahead > 0:
        semantic_ersion += f".post{num_commits_ahead}"

    if ersion.endswith('+'):  # Dirty working tree
        now = datetime.now()
        semantic_ersion += f".dev{now.strftime('%H%M')}"

    print(semantic_ersion, end='')

if __name__ == '__main__':
    main()
