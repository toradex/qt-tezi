FROM debian:13

RUN apt-get -y update && apt-get install -y --no-install-recommends \
    build-essential \
    file \
    git \
    python3 \
    xz-utils \
    qtbase5-dev \
    && apt-get clean && apt-get autoremove && rm -rf /var/lib/apt/lists/*

COPY oecore-meta-toolchain-tezi-x86_64-cortexa7t2hf-neon-colibri-imx7-toolchain-nodistro.0.sh .
RUN ./oecore-meta-toolchain-tezi-x86_64-cortexa7t2hf-neon-colibri-imx7-toolchain-nodistro.0.sh -d /usr/local/oecore-x86_64 && rm oecore-meta-toolchain-tezi-x86_64-cortexa7t2hf-neon-colibri-imx7-toolchain-nodistro.0.sh
