#!/bin/bash

PYTHON_VERSION=${PYTHON_VERSION:-3.6.6}
source "$(dirname "$0")/build_common.sh"

SOURCE_DIR=$(pwd)
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# make sure sudo is available
if [ "$EUID" -eq 0 ]; then
	apt-get -qq update
	apt-get -qq install sudo	
fi

sudo apt-get -qq update
sudo apt-get -qq install git 

# //////////////////////////////////////////////////////
function DetectUbuntuVersion {
	if [ -f /etc/os-release ]; then
		source /etc/os-release
		export OS_VERSION=$VERSION_ID

	elif type lsb_release >/dev/null 2>&1; then
		export OS_VERSION=$(lsb_release -sr)

	elif [ -f /etc/lsb-release ]; then
		source /etc/lsb-release
		export OS_VERSION=$DISTRIB_RELEASE

	fi
	echo "OS_VERSION ${OS_VERSION}"
}


DetectUbuntuVersion

# //////////////////////////////////////////////////////
function InstallCMake {

	# already exists?
	if [ -x "$(command -v cmake)" ] ; then
		CMAKE_VERSION=$(cmake --version | cut -d' ' -f3)
		CMAKE_VERSION=${CMAKE_VERSION:0:1}
		if (( CMAKE_VERSION >=3 )); then
			return
		fi	
	fi

	echo "Downloading precompiled cmake"
	DownloadFile "http://www.cmake.org/files/v3.4/cmake-3.4.3-Linux-x86_64.tar.gz"
	tar xvzf cmake-3.4.3-Linux-x86_64.tar.gz -C ${CACHED_DIR} --strip-components=1 
}


# //////////////////////////////////////////////////////
function InstallPatchElf {

	# already exists?
	if [ -x "$(command -v patchelf)" ]; then
		return
	fi
	
	echo "Compiling patchelf"
	DownloadFile https://nixos.org/releases/patchelf/patchelf-0.9/patchelf-0.9.tar.gz 
	tar xvzf patchelf-0.9.tar.gz
	pushd patchelf-0.9
	./configure --prefix=${CACHED_DIR} && make -s && make install
	autoreconf -f -i
	./configure --prefix=${CACHED_DIR} && make -s && make install
	popd
}


if (( ${OS_VERSION:0:2}<=14 )); then
	sudo apt-get -qq install software-properties-common
	sudo add-apt-repository -y ppa:deadsnakes/ppa
	sudo apt-get -qq update
fi

sudo apt-get -qq install --allow-unauthenticated cmake swig3.0 git bzip2 ca-certificates build-essential libssl-dev uuid-dev curl automake
sudo apt-get -qq install apache2 apache2-dev

InstallCMake
InstallPatchElf
InstallPython 

if (( VISUS_INTERNAL_DEFAULT == 0 )); then 
	sudo apt-get -qq install zlib1g-dev liblz4-dev libtinyxml-dev libfreeimage-dev libssl-dev libcurl4-openssl-dev
fi

# install qt (it's a version available on PyQt5)
if (( VISUS_GUI==1 )); then

	sudo apt-get install --reinstall ca-certificates
	QT_VERSION=5.11.2 # pick a version available on PyQt

	if (( ${OS_VERSION:0:2} <=14 )); then

		sudo add-apt-repository ppa:forkotov02/opt-qt-${QT_VERSION}-trusty -y
		sudo apt-get -qq update 
		sudo apt-get install -yqq mesa-common-dev libgl1-mesa-dev libglu1-mesa-dev qt511base 

	elif (( ${OS_VERSION:0:2} <=16 )); then	

		sudo add-apt-repository ppa:beineri/opt-qt-${QT_VERSION}-xenial -y
		sudo apt-get -qq update 
		sudo apt-get install -yqq mesa-common-dev libgl1-mesa-dev libglu1-mesa-dev qt511-meta-full

	elif (( ${OS_VERSION:0:2} <=18 )); then

		sudo add-apt-repository ppa:beineri/opt-qt-${QT_VERSION}-bionic -y
		sudo apt-get -qq update 
		sudo apt-get install -yqq mesa-common-dev libgl1-mesa-dev libglu1-mesa-dev qt511-meta-full

	fi

	# temporary disable exit
	set +e 
	source /opt/qt511/bin/qt511-env.sh
	export Qt5_DIR=/opt/qt511/lib/cmake/Qt5
	set -e 

fi

PushCMakeOptions
PushCMakeOption SWIG_EXECUTABLE $(which swig3.0)
cmake ${cmake_opts} ${SOURCE_DIR} 
 
cmake --build . --target all -- -j 4
cmake --build . --target test
cmake --build . --target install



