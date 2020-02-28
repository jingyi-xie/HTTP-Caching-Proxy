#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

make clean
make

printf "######################################################################
########################## ${GREEN}Before Testing${NC} ############################
######################################################################
# Please disconnect any client that is trying to connecect to this   #
# proxy, e.g. your browser, because we are going to show some log    #
# to you in stdout that proves this program works properly, having   #
# other clients connected will make this log ugly                    #
######################################################################\n\n"

read -p "After kill all the clients, press enter to continue..."
echo 'clearing caches...'
rm -rf __cache__
echo ''

printf "######################################################################
########################## ${GREEN}Start Testing${NC} #############################
######################################################################\n\n"

echo 'start running proxy server...(wait 3 seconds here)'
./main demo & demoPid=$!
sleep 3
echo 'proxy started!'
echo ''











# ------------------------------------- WRITE TEST CASES HERE --------------------------------
printf "${GREEN}1. normal request, expected HTTP 200${NC}\n"
sleep 1
echo "$(cat testCases/req1.txt)"
nc localhost 1234 < testCases/req1.txt > /dev/null 2>&1

# ------------------------------------- END TEST CASES ---------------------------------------















printf "######################################################################
########################## ${GREEN}End Testing${NC} ###############################
######################################################################\n\n"
printf "killing demo process with pid $demoPid\n"
kill "$demoPid"


printf "######################################################################
##################### ${GREEN}Start Proxy As Daemon${NC} ##########################
######################################################################\n\n"

echo 'run ./main'
./main


printf "######################################################################
######################## ${GREEN}Proxy Is Running${NC} ############################
######################################################################\n\n"

while true ; do continue ; done