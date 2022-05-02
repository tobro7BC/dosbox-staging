#!/bin/bash

set -e

get_latest_runner_version()
{
    local vers=
    vers=$(curl -s https://api.github.com/repos/actions/runner/releases/latest | jq -r .tag_name)
    printf "%s\n" "${vers#v}"
}

get_runner_download_url()
{
    local dl_type="$1"

    if [ "$RUNNER_OS" != "linux" ]; then
        echo "Unsupported OS"
        return 1
    fi
    local gh_arch=

    case "$RUNNER_ARCH" in
        arm|arm64) 
            gh_arch="$RUNNER_ARCH" ;; 
        amd64) 
            gh_arch="x64" ;;
        *) 
            echo "Unsupported arch" 
            return 1
            ;;
    esac

    runner_vers=$(curl -s https://api.github.com/repos/actions/runner/releases/latest | jq -r .tag_name)

    case "$dl_type" in
        full) 
            printf "%s\n" "https://github.com/actions/runner/releases/download/${runner_vers}/actions-runner-linux-${gh_arch}-${runner_vers#v}.tar.gz"
            ;;
        min)
            printf "%s\n" "https://github.com/actions/runner/releases/download/${runner_vers}/actions-runner-linux-${gh_arch}-${runner_vers#v}-noruntime-noexternals.tar.gz"
            ;;
        *)
            echo "DL type must be one of 'full' or 'min'"
            return 1
            ;;
    esac
    return 0
}
