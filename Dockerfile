# Build stage
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config

WORKDIR /app
COPY . .

RUN cmake -E make_directory build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --target routeforge_server -j $(nproc)

# Runtime stage
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -ms /bin/bash routeforge
USER routeforge
WORKDIR /home/routeforge

COPY --from=builder /app/build/routeforge_server /home/routeforge/routeforge_server
COPY --from=builder /app/sql /home/routeforge/sql

EXPOSE 8080
ENTRYPOINT ["./routeforge_server"]
