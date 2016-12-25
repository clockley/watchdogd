#!/bin/bash
. /etc/os-release

if (($? != 0))
then
	echo "Unable to detect disto version."
	exit 1;
fi


SUDO=''
if (( $EUID != 0))
then
	SUDO='sudo'
fi

if [ $ID == "fedora" ]
then
	if [[ "wheel" != $(groups|grep -o wheel) ]]
	then
		su -c 'dnf -y install libconfig-devel zlib-devel automake autoconf libmount-devel gcc make liboping-devel systemd-devel dbus-devel gcc-c++'
		exit
	fi
	$SUDO dnf -y install libconfig-devel zlib-devel automake autoconf libmount-devel gcc make liboping-devel systemd-devel gcc-c++
	exit
fi

if [ $ID_LIKE == "fedora" ]
then
	if [[ "wheel" != $(groups|grep -o wheel) ]]
	then
		su -c 'dnf -y install libconfig-devel zlib-devel automake autoconf libmount-devel gcc make liboping-devel systemd-devel dbus-devel gcc-c++'
		exit
	fi
	$SUDO dnf -y install libconfig-devel zlib-devel automake autoconf libmount-devel gcc make liboping-devel systemd-devel gcc-c++
	exit
fi

if [ $ID == "ubuntu" ]
then
	$SUDO apt-get -y install libconfig-dev liboping-dev zlib1g-dev automake autoconf libsystemd-dev libmount-dev gcc make libdbus-1-dev g++
	exit
fi

if [ $ID == "debian" ]
then
	$SUDO apt-get -y install libconfig-dev liboping-dev zlib1g-dev automake autoconf libsystemd-dev libmount-dev gcc make  libdbus-1-dev g++
	exit
fi

if [ $ID == "opensuse" ]
then
	su -c 'zypper install -yl libconfig-devel zlib-devel automake autoconf libmount-devel gcc make liboping-devel systemd-devel   dbus-1-devel gcc-c++'
	exit
fi

if [[ -v ID_LIKE ]]
then
	for i in $ID_LIKE
	do
		if [ $i == "debian" ]
		then
			$SUDO apt-get -y install libconfig-dev liboping-dev zlib1g-dev automake autoconf libsystemd-dev libmount-dev gcc make  libdbus-1-dev g++
			exit
		fi

		if [ $i == "ubuntu" ]
		then
			$SUDO apt-get -y install libconfig-dev liboping-dev zlib1g-dev automake autoconf libsystemd-dev libmount-dev gcc make libdbus-1-dev g++
			exit
		fi
	done
fi

echo "Your version of linux is not supported"
echo "If your are able to install this program on the version of linux you are"
echo "running please email the commands used. clockley1@gmail.com"
