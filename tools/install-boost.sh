#!/bin/bash
# Downloads sources to ~/boost-src, installs to /opt/boost

set -e

# Expected SHA256 of the release tarball
BOOST_SHA256=c4a3b310ddd2472416e091067166b0713be97c63f38c212c484ada022fd296ce

# Get the source code
mkdir ~/boost-src
cd ~/boost-src
wget -q https://archives.boost.io/release/1.92.0/source/boost_1_92_0.tar.gz
echo "$BOOST_SHA256  boost_1_92_0.tar.gz" | sha256sum -c --strict -
tar -xf boost_1_92_0.tar.gz
cd boost_1_92_0

# Build and install. Make sure you've got write access to /opt/boost,
# otherwise change the --prefix argument
./bootstrap.sh
./b2 --with-json --with-context --with-regex --with-url --with-test --with-charconv -d0 --prefix=/opt/boost install
rm -rf ~/boost-src
