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
	$SUDO dnf -y install libconfig-devel zlib-devel automake autoconf libmount-devel gcc make liboping-devel systemd-devel || $SUDO yum -y install libconfig-devel zlib-devel automake autoconf libmount-devel gcc make liboping-devel systemd-devel
	exit
fi

if [ $ID == "ubuntu" ]
then
	$SUDO apt-get -y install libconfig-dev liboping-dev zlib1g-dev automake autoconf libsystemd-dev libmount-dev gcc make
	exit
fi

if [ $ID == "debian" ]
then
	$SUDO apt-get -y install libconfig-dev liboping-dev zlib1g-dev automake autoconf libsystemd-dev libmount-dev gcc make
	exit
fi

if [ $ID == "opensuse" ]
then
	$SUDO zypper install -yl libconfig-devel zlib-devel automake autoconf libmount-devel gcc make liboping-devel systemd-devel
	exit
fi

if [[ -v ID_LIKE ]]
then
	if [ $ID_LIKE == "debian" ]
	then
		$SUDO apt-get -y install libconfig-dev liboping-dev zlib1g-dev automake autoconf libsystemd-dev libmount-dev gcc make
		exit
	fi
fi

echo "Your version of linux is not supported"
echo "If your are able to install this program on the version of linux you are"
echo "running please email the commands used. clockley1@gmail.com"
