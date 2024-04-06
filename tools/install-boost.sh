# Downloads sources to ~/boost-src, installs to /opt/boost
BOOST_MAJOR=1
BOOST_MINOR=85
BOOST_PATCH=0

# Get the source code
mkdir ~/boost-src
cd ~/boost-src
BOOST_VERSION_UNDERSCORE="${BOOST_MAJOR}_${BOOST_MINOR}_${BOOST_PATCH}"
wget -q https://boostorg.jfrog.io/artifactory/main/release/${BOOST_MAJOR}.${BOOST_MINOR}.${BOOST_PATCH}/source/boost_${BOOST_VERSION_UNDERSCORE}_rc1.tar.gz
tar -xf boost_${BOOST_VERSION_UNDERSCORE}_rc1.tar.gz
cd boost_$BOOST_VERSION_UNDERSCORE

# Build and install. Make sure you've got write access to /opt/boost,
# otherwise change the --prefix argument
./bootstrap.sh
./b2 --with-json --with-context --with-regex --with-url --with-test --with-charconv -d0 --prefix=/opt/boost install
rm -rf ~/boost-src
