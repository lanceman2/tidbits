#!/bin/bash

set -e # exit and spew on error

cd $(dirname ${BASH_SOURCE[0]})

script_dir=$PWD


# We try to write this script such that you may use your computer to do
# other things other than run the EOS software.  The install scripts in
# the eos repository use ${HOME} as the start of a prefix to install many
# of the dependencies; and that was acceptable for me.


###########################################################################
#######################       configuration      ##########################
###########################################################################

# Sat Feb 16 17:08:43 EST 2019 on my ubuntu 16.04.5 system

# Most of this is developer configuration. For example a user will likely
# not want to change the version of a dependency package, unless they have
# time to burn checking for bugs.


# INSTALL_ROOT is where this script will install the different packages,
# each in a different sub-directory with the name being the package
# tarname with version numbers.
INSTALL_ROOT=/usr/local/encap


#POST_INSTALL_COMMAND="Command to run between installing dependencies."
#POST_INSTALL_COMMAND="sudo encap"
#
# Example run: bash_prompt% POST_INSTALL_COMMAND="sudo encap" NPROC=2 time ./install_eosio.bash
#

# Notes:
pkg_depends=(clang-4.0 lldb-4.0 libclang-4.0-dev cmake make automake\
 libbz2-dev libssl-dev libgmp3-dev autotools-dev build-essential\
 libicu-dev python2.7-dev python3-dev autoconf libtool curl zlib1g-dev\
 doxygen graphviz lcov)


# We don't encourage you to install packages that are not managed by the
# OS package manager as root.
#
# The decision to run as root should be decided at a higher level then
# this script.  Passing that decision to a higher level makes this script
# much more usable and less buggy, and less prone to braking the system.
# Think about it...
#
# Adding "sudo" to a script of this type is bad form; therefore this next
# commented line below needs to be in a different script, or should be run
# "by hand".
#
#apt-get install ${pkg_depends[*]}
#exit


# The EOS.IO github repo tag
EOS_TAG=v1.6.0



# Debug|Release|RelWithDebInfo|MinSizeRel
EOS_BUILD_TYPE=Debug

#true|false
EOS_DOXYGEN=false

EOS_CXX_COMPILER=clang++-4.0

EOS_C_COMPILER=clang-4.0

EOS_ENABLE_COVERAGE_TESTING=true



# Dependency package source (or binary) URLS for packages that may not be
# able to be installed via the OS package manager (like APT or yum).

# We don't encourage you to install packages that are not managed by the
# OS package manager as root, for oh so many reasons.  If you must install
# software that sucks so badly that it requires root to install use
# chroot.  Most free software developers know better, but some do not.


# does apt has this boost_1_67_0? I don't know. Seeing:
#dpkg -S /usr/include/boost/version.hpp 
# libboost1.58-dev:amd64: /usr/include/boost/version.hpp


# So ya, changing any of the "package version"-like variables just below
# may expose a porting/compatibility bugs between the various packages.
#
# One might argue that in most cases that any bug lies in the dependent
# and/or least mature package in the shortest list of packages that it
# take to manifest the bug.


# These package dependencies have mongo_cxx depend on mongo-c.

# In some cases the tarname is not the some as the directory that it
# untars to (tardir).


boost_tarname=boost_1_67_0
boost_tar=$boost_tarname.tar.bz2
boost_tardir=$boost_tarname
boost_url=https://dl.bintray.com/boostorg/release/1.67.0/source/$boost_tar

mongodb_tarname=mongodb-linux-x86_64-3.6.3
mongodb_tar=$mongodb_tarname.tgz
mongodb_tardir=$mongodb_tarname
mongodb_url="https://fastdl.mongodb.org/linux/$mongodb_tar"

mongo_c_tarname=mongo-c-driver-1.10.2
mongo_c_tar=$mongo_c_tarname.tar.gz
mongo_c_tardir=$mongo_c_tarname
mongo_c_url=https://github.com/mongodb/mongo-c-driver/releases/download/1.10.2/$mongo_c_tar

mongo_cxx_tarname=mongo-cxx-driver-r3.3.0
mongo_cxx_tar=$mongo_cxx_tarname.tar.gz
mongo_cxx_tardir=mongodb-mongo-cxx-driver-9bdb482
mongo_cxx_url=https://github.com/mongodb/mongo-cxx-driver/tarball/r3.3.0

llvm_tag=release_80

# More so, developer level configuration below here, not that the above
# parameter variables were so obvious.


# We put a file with this filename in each package top source untar
# directory, after the it finished installing, so we can exclude doing it
# again on the next try.  If you want to reinstall a package again just
# "rm -rf" the untared source directory, and leave the tarball in this top
# directory.
successFile=SUCCESSFULLY_INSTALLED


build_dir=$script_dir/BUILD

nproc=${NPROC:=$(nproc||echo '3')}


#########################################################################
##################### END configuration #################################
#########################################################################


# NOTES: This script is designed work without being run as a super user
# (root or sudo) and that's a good thing.

# First checks that can fail and exit:

# This script ports: currently very limited.
if [ "$(lsb_release -si)" != "Ubuntu" ] ||\
    [ "$(lsb_release -sr)" != "16.04" ] ; then
    cat << EOF
    $(basename $0) is not ported to: "$(lsb_release\
    -sd 2>/dev/null)"
EOF
    exit 1
fi

echo "using nproc=$nproc threads to build with"


# We assume that BUILD_LOG_FILE_xx is only used by this script, if it is
# used already then the only effect will be is that we will not write a
# log file.
#
# Relaunch with logging file if not already:
if [ -z "$BUILD_LOG_FILE_xx" ] ; then
    count=1
    while [ -f LOG_$count ] ; do
        let count=$count+1
    done
    export BUILD_LOG_FILE_xx=LOG_$count
    echo "Writing log file $BUILD_LOG_FILE_xx"
    # log stderr and stdout in this directory in LOG_$count
    exec $0 2>&1 | tee LOG_$count

    exit 1 # should not get here.
fi



[ -d "$build_dir" ] &&\
 echo "Good: top build directory \"$build_dir\" already exists"

mkdir -p $build_dir
cd $build_dir

##########################################################################
# We separate all the downloading from the building and installing.  Doing
# so lets the user download now, and build/install later without a network
# connection.  And the if there is an error they do not have to download
# again and again in order to build and install.
##########################################################################

echo "Downloading sources"

set -x # show what we run

for name in boost mongodb mongo_c mongo_cxx ; do
    tar=${name}_tar
    url=${name}_url
    tar=${!tar}
    url=${!url}
    if [ ! -f "${tar}" ] ; then
        # download
        wget -O $tar $url
    fi
    # check the sha512 hash (if we have it) of the downloaded file:
    if [ -f ../$tar.sha512 ] ; then
        sha512sum -c ../$tar.sha512
    else
        sha512sum $tar
    fi
done


if [ ! -d llvm-$llvm_tag/.git ] ; then
    # Get the code for LLVM with WASM support
    git clone --depth 1 --single-branch --branch $llvm_tag https://github.com/llvm-mirror/llvm.git llvm-$llvm_tag
    cd llvm-$llvm_tag/tools
    git clone --depth 1 --single-branch --branch $llvm_tag https://github.com/llvm-mirror/clang.git
    cd $build_dir
fi



# TODO: Get just a particular tagged version tarball of this eos code and
# not the whole repository.  Even with depth 1 the repo can double the
# size of all the files.  Same for llvm above.
#
if [ -f eos/EOS_TAG ] && [ "$(cat eos/EOS_TAG)" = "$EOS_TAG" ]
then
    set +x
    echo "Good: EOS version $EOS_TAG has already been downloaded"
    set -x
elif [ ! -d eos ] ; then
    git clone --recursive --depth 1 --single-branch --branch $EOS_TAG https://github.com/EOSIO/eos
    set +x
    echo "$EOS_TAG" > eos/EOS_TAG # just so we mark it
    set -x
else
    set +x
    echo "eos/ already exists and is not from this version of this script"
    exit 1
fi


echo "Finished Downloading (and checking) sources"


if [ ! -f $boost_tardir/SUCCESSFULLY_INSTALLED ] ; then
    set +x # do not show echo
    echo "Building and installing boost: $boost_tarname"
    set -x # turn on show again
    prefix=$INSTALL_ROOT/$boost_tarname
    rm -rf $boost_tardir $prefix
    tar xjf $boost_tar
    cd $boost_tardir
    ./bootstrap.sh --prefix=$prefix
    ./b2 -j $nproc install
    echo "$boost_tarname installed in $prefix" > SUCCESSFULLY_INSTALLED
    cd $build_dir
    $POST_INSTALL_COMMAND
fi

mongodb_prefix=$INSTALL_ROOT/$mongodb_tardir

if [ ! -f $INSTALL_ROOT/$mongodb_tarname/SUCCESSFULLY_INSTALLED ] ; then
    set +x
    echo "Installing mongodb binary: $mongodb_tarname"
    set -x
    cd $INSTALL_ROOT
    tar -xzf $build_dir/$mongodb_tar
    echo "$mongodb_tarname installed in $mongodb_prefix" > $mongodb_tarname/SUCCESSFULLY_INSTALLED
    cd $build_dir
    $POST_INSTALL_COMMAND
fi


if [ ! -f $mongo_c_tardir/SUCCESSFULLY_INSTALLED ] ; then
    set +x
    echo "Building and installing $mongo_c_tarname"
    set -x
    prefix=$INSTALL_ROOT/$mongo_c_tarname
    rm -rf $mongo_c_tardir $prefix
    tar -xzf $build_dir/$mongo_c_tar
    cd $mongo_c_tardir
    mkdir BUILD_XXX
    cd BUILD_XXX
    cmake\
 -DCMAKE_BUILD_TYPE=Release\
 -DCMAKE_INSTALL_PREFIX=$prefix\
 -DENABLE_BSON=ON -DENABLE_SSL=OPENSSL\
 -DENABLE_AUTOMATIC_INIT_AND_CLEANUP=OFF\
 -DENABLE_STATIC=ON\
 ..
    make VERBOSE=1 -j $nproc
    make install
    echo "$mongo_c_tarname installed in $prefix" > ../SUCCESSFULLY_INSTALLED
    cd $build_dir
    $POST_INSTALL_COMMAND
fi


# TODO: Skip mongod.conf if done already?
# Not so sure about any of this mongod.conf.
cat > $INSTALL_ROOT/$mongodb_tarname/mongod.conf <<mongodconf
systemLog:
 destination: file
 path: $INSTALL_ROOT/$mongodb_tarname/log/mongodb.log
 logAppend: true
 logRotate: reopen
net:
 bindIp: 127.0.0.1,::1
 ipv6: true
storage:
 dbPath: $INSTALL_ROOT/$mongodb_tarname/data
mongodconf


if [ ! -f $mongo_cxx_tardir/SUCCESSFULLY_INSTALLED ] ; then
    set +x
    echo "Building and installing $mongo_cxx_tarname"
    set -x
    prefix=$INSTALL_ROOT/$mongo_cxx_tarname
    rm -rf $mongo_cxx_tardir $prefix
    tar -xzf $build_dir/$mongo_cxx_tar
    cd $mongo_cxx_tardir
    mkdir BUILD_XXX
    cd BUILD_XXX
    cmake\
 -DBUILD_SHARED_LIBS=OFF\
 -DCMAKE_BUILD_TYPE=Release\
 -DCMAKE_INSTALL_PREFIX=$prefix\
 ..
    make VERBOSE=1 -j $nproc
    make install
    echo "$mongo_cxx_tarname installed in $prefix" > ../SUCCESSFULLY_INSTALLED
    cd $build_dir
    $POST_INSTALL_COMMAND
fi


llvm_prefix=$INSTALL_ROOT/llvm-$llvm_tag


if [ ! -f llvm/SUCCESSFULLY_INSTALLED ] ; then
    set +x
    echo "Building and installing llvm"
    set -x
    rm -rf $llvm_prefix llvm-$llvm_tag/BUILD_XXX
    cd llvm-$llvm_tag
    mkdir BUILD_XXX
    cd BUILD_XXX
    cmake\
 -G "Unix Makefiles"\
 -DCMAKE_INSTALL_PREFIX=$llvm_prefix\
 -DLLVM_TARGETS_TO_BUILD=\
 -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=WebAssembly\
 -DCMAKE_BUILD_TYPE=Release\
 ..
    make VERBOSE=1 -j $nproc
    make install
    echo "llvm-$llvm_tag installed in $llvm_prefix" > ../SUCCESSFULLY_INSTALLED
    cd $build_dir
    $POST_INSTALL_COMMAND
fi


if [ ! -f eos/SUCCESSFULLY_INSTALLED ] ; then
    prefix=$INSTALL_ROOT/eosio-$EOS_TAG
    rm -rf eos/BUILD_XXX $prefix
    mkdir eos/BUILD_XXX
    cd eos/BUILD_XXX
    export LLVM_DIR=$llvm_prefix/lib/cmake/llvm
    cmake\
 -DCMAKE_BUILD_TYPE=$EOS_BUILD_TYPE\
 -DCMAKE_CXX_COMPILER="${EOS_CXX_COMPILER}"\
 -DCMAKE_C_COMPILER="${EOS_C_COMPILER}"\
 -DWASM_ROOT=$llvm_prefix\
 -DCORE_SYMBOL_NAME=SYS\
 -DOPENSSL_ROOT_DIR=/usr/include/openssl\
 -DBUILD_MONGO_DB_PLUGIN=true\
 -DENABLE_COVERAGE_TESTING=$EOS_ENABLE_COVERAGE_TESTING\
 -DBUILD_DOXYGEN=$EOS_DOXYGEN\
 -DCMAKE_INSTALL_PREFIX=$prefix\
 -DBUILD_DOXYGEN=true\
 ..
    make VERBOSE=1
    make install
    echo "eos installed in $prefix" > ../SUCCESSFULLY_INSTALLED
    cd $build_dir
    $POST_INSTALL_COMMAND
fi


echo "Successfully made it to the end of script $0"
