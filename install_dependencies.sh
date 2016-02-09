#!/bin/bash

distro=$(lsb_release -i|cut -f 2 -d ":"|tr -d "\t")

SUDO=''
if (( $EUID != 0))
then
	SUDO='sudo'
fi

if [ $distro == "Fedora" ]
then
	$SUDO dnf -y install libconfig-devel zlib-devel automake autoconf libmount-devel gcc make liboping-devel systemd-devel || $SUDO yum -y install libconfig-devel zlib-devel automake autoconf libmount-devel gcc make liboping-devel systemd-devel
	exit
fi

if [ $distro == "Ubuntu" ]
then
	$SUDO apt-get -y install libconfig-dev liboping-dev zlib1g-dev automake autoconf libsystemd-dev libmount-dev gcc make
	exit
fi

echo "Your version of linux is not supported"
echo "If your are able to install this program on the version of linux you are"
echo "running please email the commands used. clockley1@gmail.com"
