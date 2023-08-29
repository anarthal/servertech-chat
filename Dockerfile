#
# Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

#
# Build the server
#
FROM alpine:3.18.2 AS server-builder-base

# Packages
RUN apk update && \
    apk add --no-cache \
        g++ \
        cmake \
        ninja \
        openssl-dev \
        git \
        linux-headers \
        wget

# Boost
WORKDIR /boost-src
RUN \
    REDIS_COMMIT=f506e1baee4941bff1f8e2f3aa7e1b9cf08cb199 && \
    wget -q https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz && \
    tar -xf boost_1_82_0.tar.gz && \
    cd boost_1_82_0 && \
    git clone --depth 1 https://github.com/boostorg/redis.git libs/redis && \
    cd libs/redis && \
    git fetch origin $REDIS_COMMIT && \
    git checkout $REDIS_COMMIT && \
    cd /boost-src && \
    ./bootstrap.sh && \
    ./b2 --with-json --with-context --with-test -d0 --prefix=/boost install && \
    cd / && \
    rm -rf /boost-src


# Copy the server files
WORKDIR /app
COPY server/ ./

#
# Build the actual server
#
FROM server-builder-base AS server-builder
WORKDIR /app/__build
RUN cmake -DCMAKE_GENERATOR=Ninja -DCMAKE_PREFIX_PATH=/boost -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF .. && \
    cmake --build . --parallel 8 && \
    cp main /app/main && \
    rm -rf /app/__build

#
# Build the server tests. This is an optional step
#
FROM server-builder-base AS server-tests
WORKDIR /app/__build
RUN cmake -DCMAKE_GENERATOR=Ninja -DCMAKE_PREFIX_PATH=/boost -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON .. && \
    cmake --build . --parallel 8 && \
    ctest --output-on-failure && \
    rm -rf /app/__build

#
# Build the client - setup
#
FROM node:18-alpine AS client-builder-base

# Don't send telemetry
ENV NEXT_TELEMETRY_DISABLED 1

# OS dependencies
RUN apk add --no-cache libc6-compat

# Application dependencies
WORKDIR /app
COPY client/package.json client/package-lock.json ./
RUN npm ci

# Application code
COPY client ./

#
# Actually build the client
#
FROM client-builder-base AS client-builder
RUN npm run build

#
# Client tests. This is an optional step
#
FROM client-builder-base AS client-tests

# Run the tests
RUN npm run test

#
# Runtime image. curl is required for health checks
#
FROM alpine:3.18.2
RUN apk add openssl libstdc++ curl
COPY --from=server-builder \
    /boost/lib/libboost_container.so* \
    /boost/lib/libboost_context.so* \
    /boost/lib/libboost_json.so* \
    /boost/lib/
COPY --from=server-builder /app/main /app/
COPY --from=client-builder /app/out/ /app/static/

EXPOSE 80
ENTRYPOINT [ "/app/main", "0.0.0.0", "80", "/app/static/" ]
