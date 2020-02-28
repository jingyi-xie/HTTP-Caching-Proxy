#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

make clean
make

printf "${GREEN}1"
echo '######################################################################'
echo '########################## Before Testing ############################'
echo '######################################################################'
echo '# Please disconnect any client that is trying to connecect to this   #'
echo '# proxy, e.g. your browser, because we are going to show some log    #'
echo '# to you in stdout that proves this program works properly, having   #'
echo '# other clients connected will make this log ugly                    #'
echo '######################################################################'
printf "${NC}"

echo ''
read -p "After kill all the clients, press enter to continue..."
echo 'clearing caches...'
rm -rf __cache__
echo ''

printf "${GREEN}"
echo '######################################################################'
echo '########################## Start Testing #############################'
echo '######################################################################'
echo ''
printf "${NC}"

echo 'start running proxy server...(wait 3 seconds here)'
./main demo & demoPid=$!
sleep 3
echo 'proxy started!'
echo ''











# ------------------------------------- WRITE TEST CASES HERE --------------------------------
printf "${GREEN}1. normal request, expected HTTP 200${NC}\n"
sleep 1
echo "$(cat testCases/req1.txt)"
nc localhost 1234 < testCases/req1.txt

# ------------------------------------- END TEST CASES ---------------------------------------















printf "${GREEN}"
echo '######################################################################'
echo '########################## End Testing ###############################'
echo '######################################################################'
echo ''
printf "${NC}"
printf "killing demo process with pid $demoPid"
kill "$demoPid"


printf "${GREEN}"
echo '######################################################################'
echo '##################### Start Proxy As Daemon ##########################'
echo '######################################################################'
echo ''
printf "${NC}"

echo 'run ./main'
./main


printf "${GREEN}"
echo '######################################################################'
echo '######################## Proxy Is Running ############################'
echo '######################################################################'
echo ''
printf "${NC}"

while true ; do continue ; done