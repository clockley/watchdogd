#!/bin/bash
spinner()
{
    local pid=$!
    local delay=0.5
    local spinstr='|/-\'
    while [ "$(ps a | awk '{print $1}' | grep $pid)" ]; do
        local temp=${spinstr#?}
        printf " [%c]  " "$spinstr"
        local spinstr=$temp${spinstr%"$temp"}
        sleep $delay
        printf "\b\b\b\b\b\b"
    done
    printf "    \b\b\b\b"
}

(autoreconf -fi; rm -rf autom4te.cache) & spinner
printf "\n\tCompleted installing autoconf files into project.\n"
printf "\tTo install watchdogd into your system type the following command:\n"
printf "\t./install_dependencies.sh;./configure;make;sudo make install\n\n"
