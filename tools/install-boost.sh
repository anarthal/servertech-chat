# Downloads sources to ~/boost-src, installs to /opt/boost
# Get the source code
mkdir ~/boost-src
cd ~/boost-src
wget -q https://archives.boost.io/release/1.89.0/source/boost_1_89_0.tar.gz
tar -xf boost_1_89_0.tar.gz
cd boost_1_89_0

# Build and install. Make sure you've got write access to /opt/boost,
# otherwise change the --prefix argument
./bootstrap.sh
./b2 --with-json --with-context --with-regex --with-url --with-test --with-charconv -d0 --prefix=/opt/boost install
rm -rf ~/boost-src
