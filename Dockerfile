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
        icu-dev \
        git \
        linux-headers \
        wget

# Boost. Boost.Redis hasn't been integrated into Boost (yet), so we need to
# manually put it into $BOOST_ROOT/libs before building Boost. The script
# removes intermediate build files to make CI caching lighter
COPY tools/install-boost.sh .
RUN sh -e install-boost.sh

# Copy the server files
WORKDIR /app
COPY server/ ./

#
# Build the actual server
#
FROM server-builder-base AS server-builder
WORKDIR /app/__build
RUN cmake -DCMAKE_GENERATOR=Ninja -DCMAKE_PREFIX_PATH=/opt/boost -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF .. && \
    cmake --build . --parallel 8 && \
    cp main /app/main && \
    rm -rf /app/__build

#
# Build the server tests. This is an optional step
#
FROM server-builder-base AS server-tests
WORKDIR /app/__build
RUN cmake -DCMAKE_GENERATOR=Ninja -DCMAKE_PREFIX_PATH=/opt/boost -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON .. && \
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
RUN npm run test

#
# Runtime image
#
FROM alpine:3.18.2
RUN apk add openssl icu libstdc++ curl
COPY --from=server-builder \
    /opt/boost/lib/libboost_container.so* \
    /opt/boost/lib/libboost_context.so* \
    /opt/boost/lib/libboost_json.so* \
    /opt/boost/lib/libboost_regex.so* \
    /opt/boost/lib/libboost_url.so* \
    /opt/boost/lib/
COPY --from=server-builder /app/main /app/
COPY --from=client-builder /app/out/ /app/static/

EXPOSE 80
ENTRYPOINT [ "/app/main", "0.0.0.0", "80", "/app/static/" ]
HEALTHCHECK --start-period=5s CMD [ "curl", "-f", "http://localhost/" ]
