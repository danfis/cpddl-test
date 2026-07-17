#!/bin/bash
# Fetches the differing regression outputs from a remote copy of the tests
# repo (where a test run was just performed) and extracts them into the local
# reg/ tree, overwriting the local *.tmp files -- ready for review with
# scripts/check-reg.sh / scripts/fix-reg.sh.
#
# It runs scripts/pack-reg-diffs.sh on the remote host and pipes the resulting
# tar.gz stream into a local `tar xzf -`.
#
# Run from the tests/ directory.

set -euo pipefail

# Defaults (override with -h / -d).
HOST=deis-cluster-proxy
# Track whatever branch is currently checked out locally.  The remote shell
# expands the leading ~.
REMOTE_DIR="~/cpddl-test/$(git rev-parse --abbrev-ref HEAD)/tests"

usage() {
    cat >&2 <<EOF
Usage: $0 [-h ssh-host] [-d remote-tests-dir]

Fetch differing regression *.tmp outputs from a remote tests/ checkout and
extract them into the local reg/ tree.

  -h ssh-host          remote host to ssh into (default: $HOST)
  -d remote-tests-dir  remote tests/ directory (default: $REMOTE_DIR)
EOF
}

while getopts "h:d:?" opt; do
    case "$opt" in
        h) HOST=$OPTARG ;;
        d) REMOTE_DIR=$OPTARG ;;
        ?) usage; exit 0 ;;
    esac
done

echo "fetch-reg-diffs: fetching from $HOST:$REMOTE_DIR" >&2
ssh "$HOST" "cd $REMOTE_DIR && scripts/pack-reg-diffs.sh" | tar xzvf -
