FROM ubuntu:24.04

RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential cmake git \
        libx11-dev libxrandr-dev libgl1-mesa-dev \
        libgtk-3-dev libwebkit2gtk-4.1-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

RUN cmake -B build -S . && \
    cmake --build build --config Release --target NeuroCoreTests

CMD ["bash"]

