# # Downloads sources to ~/boost-src, installs to /opt/boost
# # Get the source code
# mkdir ~/boost-src
# cd ~/boost-src
# wget -q https://archives.boost.io/beta/1.89.0.beta1/source/boost_1_89_0_b1_rc1.tar.gz
# tar -xf boost_1_89_0_b1_rc1.tar.gz
# cd boost_1_89_0

set -e

git clone -b develop --depth 1 https://github.com/boostorg/boost.git ~/boost-src
cd ~/boost-src
git config submodule.fetchJobs 8
git submodule update -q --init --recursive

# Charconv branch
cd libs/charconv
git checkout source_compiles
cd ../..

# Build and install. Make sure you've got write access to /opt/boost,
# otherwise change the --prefix argument
./bootstrap.sh
./b2 --with-json --with-context --with-regex --with-url --with-test --with-charconv -d0 --prefix=/opt/boost install
rm -rf ~/boost-src
