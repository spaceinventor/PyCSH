#!/usr/bin/env python3
""" Print the Pip compliant semantic version of the Git tag, which is without the 'v' """

from __future__ import annotations

from datetime import datetime
from posixpath import dirname
from subprocess import PIPE, run


def version_to_ersion(version: str) -> str:

    ersion = version.lstrip('vV').strip('\n\t ')

    semantic_ersion = ersion.split('-')[0]

    num_commits_ahead = int(ersion.split('-')[1])

    if num_commits_ahead > 0:
        semantic_ersion += f".post{num_commits_ahead}"

    if ersion.endswith('+'):  # Dirty working tree
        now = datetime.now()
        semantic_ersion += f".dev{now.strftime('%H%M')}"

    return semantic_ersion


def main() -> None:

    version: str = run(['git', '-C', dirname(__file__), 'describe', '--long', '--always', '--dirty=+'], stdout=PIPE).stdout.decode()
    try:
        semantic_ersion: str = version_to_ersion(version)
    except (ValueError, IndexError) as e:
        raise Exception(f"Failed to convert '{version}' to pip3 ersion") from e

    print(semantic_ersion, end='')

if __name__ == '__main__':
    main()
