# copy

# Build the server
FROM alpine:3.18.2 AS build
RUN apk update && \
    apk add --no-cache \
        g++ \
        cmake \
        ninja \
        openssl-dev \
        git \
        linux-headers

WORKDIR /boost-src
RUN git config --global submodule.fetchJobs 8
RUN git clone --depth 1 https://github.com/boostorg/boost.git --recurse-submodules --shallow-submodules .
RUN git clone --depth 1 https://github.com/boostorg/redis.git libs/redis
RUN ./bootstrap.sh
RUN ./b2 --with-json --with-context --prefix=/boost install

WORKDIR /app
COPY src/ ./src/
COPY CMakeLists.txt .

WORKDIR /app/__build

RUN cmake -DCMAKE_GENERATOR=Ninja -DCMAKE_PREFIX_PATH=/boost -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --parallel 8

# Build the client
FROM node:18-alpine AS client-builder

# Don't send telemetry
ENV NEXT_TELEMETRY_DISABLED 1

RUN apk add --no-cache libc6-compat
WORKDIR /app
COPY client ./
RUN npm ci
RUN npm run build



# Runtime image
FROM alpine:3.18.2
RUN apk add openssl libstdc++
COPY --from=build /boost/lib /boost/lib
COPY --from=build /app/__build/main /app/
COPY --from=client-builder /app/out/ /app/static/

EXPOSE 80
ENTRYPOINT [ "/app/main", "0.0.0.0", "80", "/app/static/", "1" ]

