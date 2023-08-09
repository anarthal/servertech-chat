#
# Copyright (c) 2023 Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#

#
# Build the server
#
FROM alpine:3.18.2 AS server-builder

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
    wget -q https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz && \
    tar -xf boost_1_82_0.tar.gz

# Boost.Redis
WORKDIR /boost-src/boost_1_82_0
RUN REDIS_COMMIT=f506e1baee4941bff1f8e2f3aa7e1b9cf08cb199 && \
    git clone --depth 1 https://github.com/boostorg/redis.git libs/redis && \
    cd libs/redis && \
    git fetch origin $REDIS_COMMIT && \
    git checkout $REDIS_COMMIT

# Build
RUN \
    ./bootstrap.sh && \
    ./b2 --with-json --with-context -d0 --prefix=/boost install

# The actual server
WORKDIR /app
COPY server/ ./
WORKDIR /app/__build
RUN cmake -DCMAKE_GENERATOR=Ninja -DCMAKE_PREFIX_PATH=/boost -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . --parallel 8

#
# Build the client
#
FROM node:18-alpine AS client-builder

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
RUN npm run build


# Runtime image
FROM alpine:3.18.2
RUN apk add openssl libstdc++
COPY --from=server-builder /boost/lib /boost/lib
COPY --from=server-builder /app/__build/main /app/
COPY --from=client-builder /app/out/ /app/static/

EXPOSE 80
ENTRYPOINT [ "/app/main", "0.0.0.0", "80", "/app/static/" ]
