#!/bin/bash

set -e

. /staging-scripts/common.sh

# Register as a new runner if no existing credential files exist
if ! [ "$(ls -A ${RUNNER_CREDS_DIR})" ]; then
    if [ -z "$RUNNER_REG_TOKEN" ]; then
        echo "RUNNER_REG_TOKEN not set"
        exit 1
    fi
    if [ -z "$RUNNER_NAME" ]; then 
        echo "RUNNER_NAME not set"
        exit 1
    fi

    ./config.sh \
        --unattended \
        --url "https://github.com/${RUNNER_REPO:-"dosbox-staging/dosbox-staging"}" \
        --token "$RUNNER_REG_TOKEN" \
        --name "$RUNNER_NAME"
    
    # Move credentials to persistent volume 
    mv .credentials .credentials_rsaparams .runner "${RUNNER_CREDS_DIR}"
fi

if dpkg --compare-versions $(./config.sh --version) lt $(get_latest_runner_version); then
    # Ensure we have the latest version of the runner
    echo "Installed runner older than latest runner, updating to latest version"
    runner_url=$(get_runner_download_url min)
    rc="$?"
    if [ "$rc" = "0" ]; then
        curl -L -o ../action-runner.tar.gz "$runner_url"
        tar -xzvf ../action-runner.tar.gz ./
    fi
else
    echo "Latest version of runner already installed"
fi

# Link credentials from persistent volume to runner root dir
[ -f ${RUNNER_CREDS_DIR}/.credentials ] && ln -sf ${RUNNER_CREDS_DIR}/.credentials ./
[ -f ${RUNNER_CREDS_DIR}/.credentials_rsaparams ] && ln -sf ${RUNNER_CREDS_DIR}/.credentials_rsaparams ./
[ -f ${RUNNER_CREDS_DIR}/.runner ] && ln -sf ${RUNNER_CREDS_DIR}/.runner ./

if [ -n "$1" ]; then
    exec $@
else
    exec ./run.sh
fi
