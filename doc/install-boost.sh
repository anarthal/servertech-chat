# Downloads sources to ~/boost-src, installs to ~/boost
REDIS_COMMIT=f506e1baee4941bff1f8e2f3aa7e1b9cf08cb199

# Get the source code
mkdir ~/boost-src
cd ~/boost-src
wget -q https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz
tar -xf boost_1_82_0.tar.gz
cd boost_1_82_0

# Add Boost.Redis
git clone --depth 1 https://github.com/boostorg/redis.git libs/redis
cd libs/redis
git fetch origin $REDIS_COMMIT
git checkout $REDIS_COMMIT
cd ~/boost-src/boost_1_82_0

# Build and install
./bootstrap.sh
./b2 --with-json --with-context --with-test -d0 --prefix=$HOME/boost install
rm -rf ~/boost-src