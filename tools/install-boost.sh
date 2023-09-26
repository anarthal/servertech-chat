# Downloads sources to ~/boost-src, installs to /opt/boost
BOOST_REDIS_COMMIT=44a608c0bae467b62ce2cdf3fba409e8b0d80af2
BOOST_MAJOR=1
BOOST_MINOR=83
BOOST_PATCH=0

# Get the source code
mkdir ~/boost-src
cd ~/boost-src
BOOST_VERSION_UNDERSCORE="${BOOST_MAJOR}_${BOOST_MINOR}_${BOOST_PATCH}"
wget -q https://boostorg.jfrog.io/artifactory/main/release/${BOOST_MAJOR}.${BOOST_MINOR}.${BOOST_PATCH}/source/boost_${BOOST_VERSION_UNDERSCORE}.tar.gz
tar -xf boost_$BOOST_VERSION_UNDERSCORE.tar.gz
cd boost_$BOOST_VERSION_UNDERSCORE

# Add Boost.Redis
git clone --depth 1 https://github.com/boostorg/redis.git libs/redis
cd libs/redis
git fetch origin $BOOST_REDIS_COMMIT
git checkout $BOOST_REDIS_COMMIT
cd ~/boost-src/boost_$BOOST_VERSION_UNDERSCORE

# Build and install. Make sure you've got write access to /opt/boost,
# otherwise change the --prefix argument
./bootstrap.sh
./b2 --with-json --with-context --with-regex --with-url --with-test -d0 --prefix=/opt/boost install
rm -rf ~/boost-src
