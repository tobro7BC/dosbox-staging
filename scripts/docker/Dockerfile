FROM ubuntu:20.04

# Install general dependencies
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install --no-install-recommends -y \
        apt-transport-https \
        ca-certificates \
    && apt-get update && apt-get install --no-install-recommends -y \
        git \
        cmake \
        g++ \
        clang \
        build-essential \
        ccache \
        python3-setuptools \
        meson \
        curl \
        unzip \
        jq \
        sudo \
    && apt-get clean

# Create user for runner
ARG RUNNER_USER=staging-runner
ARG RUNNER_DIR=/home/${RUNNER_USER}/action-runner
RUN useradd -m -s /bin/bash ${RUNNER_USER}

# Allow runner user to run apt-get
RUN printf "%s\n" "${RUNNER_USER} ALL=NOPASSWD: /usr/bin/apt-get, /usr/bin/aptitude" >> \
        /etc/sudoers.d/runnerperms \
    && chmod 0440 /etc/sudoers.d/runnerperms

# Download full GH runner archive, note, only currently supporting Linux target OS
ARG TARGETARCH
ARG TARGETOS
ENV RUNNER_OS=${TARGETOS}
ENV RUNNER_ARCH=${TARGETARCH}

COPY common.sh /staging-scripts/common.sh
RUN chmod +x /staging-scripts/common.sh

RUN . /staging-scripts/common.sh \
    && runner_url=$(get_runner_download_url full) \
    && rc="$?" \
    && [ "$rc" = "0" ] \
    && mkdir -p ${RUNNER_DIR} \
    && cd "$RUNNER_DIR" \
    && curl -L -o ../action-runner.tar.gz "$runner_url" \
    && tar -xzvf ../action-runner.tar.gz ./ \
    && chown -R ${RUNNER_USER}:${RUNNER_USER} ${RUNNER_DIR} \
    && rm ../action-runner.tar.gz

# Install GH runner dependencies
RUN ${RUNNER_DIR}/bin/installdependencies.sh \
    && apt-get clean

COPY entrypoint.sh /staging-scripts/entrypoint.sh
RUN chmod +x /staging-scripts/entrypoint.sh

ENV RUNNER_CREDS_DIR=/runner_creds
RUN mkdir -p /runner_creds && chown -R ${RUNNER_USER}:${RUNNER_USER} "$RUNNER_CREDS_DIR"

USER ${RUNNER_USER}
WORKDIR ${RUNNER_DIR}

ENTRYPOINT [ "/staging-scripts/entrypoint.sh" ]
