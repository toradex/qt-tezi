FROM debian

RUN apt-get -y update && apt-get install -y --no-install-recommends \
    build-essential \
    qt5-default \
    python file git \
    && apt-get clean && apt-get autoremove && rm -rf /var/lib/apt/lists/*

COPY oecore-x86_64-armv7vet2hf-neon-toolchain-nodistro.0.sh .
RUN ./oecore-x86_64-armv7vet2hf-neon-toolchain-nodistro.0.sh -d /usr/local/oecore-x86_64 && rm oecore-x86_64-armv7vet2hf-neon-toolchain-nodistro.0.sh
