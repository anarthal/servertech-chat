#!/bin/bash
# Downloads sources to ~/boost-src, installs to /opt/boost

set -e

# Expected SHA256 of the tarball
BOOST_SHA256=8abdffb93ec9f8223c167bc6a331d114e92f89ac793b743e23b92f97cf943815

# Get the source code
mkdir ~/boost-src
cd ~/boost-src
wget -q https://archives.boost.io/beta/1.93.0.beta1/source/boost_1_93_0_b1.tar.gz
echo "$BOOST_SHA256  boost_1_93_0_b1.tar.gz" | sha256sum -c -
tar -xf boost_1_93_0_b1.tar.gz
cd boost_1_93_0

# Build and install. Make sure you've got write access to /opt/boost,
# otherwise change the --prefix argument
./bootstrap.sh
./b2 --with-json --with-context --with-regex --with-url --with-test --with-charconv -d0 --prefix=/opt/boost install
rm -rf ~/boost-src
