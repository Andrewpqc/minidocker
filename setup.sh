#!/bin/bash

# [WARN]
# this file can be run only once.

# [INFO]
# this file is to create the default 'minidockerbr'
# network bridge which help the container to communicate with the network
# of the host
# sudo apt install uml-utilities bridge-utils
# sudo brctl addbr minidockerbr

# add accept rule to iptables, to help the container to communicate with the network
# of the outside world
sudo iptables -I POSTROUTING -t nat -s 192.168.0.1/16 ! -o minidockerbr -j MASQUERADE
sudo iptables -I FORWARD -i minidockerbr -o minidockerbr -j ACCEPT
sudo iptables -I FORWARD -i minidockerbr ! -o minidockerbr -j ACCEPT
sudo iptables -I FORWARD -o minidockerbr -m conntrack --ctstate RELATED,ESTABLISHED -j ACCEPT
# sudo iptables -I FORWARD -o minidockerbr -m conntrack -j ACCEPT
# make the project
# make

# move the binary to PATH
# make install

