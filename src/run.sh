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

echo 'clearing caches...'
rm -rf __cache__/
read -p "After kill all the clients, press enter to continue..."
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
declare -a titles=("Trival GET" "First GET" "Second GET, request the same as testcase 2" "Trival POST" "ill-formatted GET" "ill-formatted POST" "GET a 404" "Re-validation")
declare -a exps=("get and cache" "get and cache" "be able to find response in cache" "do the work and not cache" "report warning and continue" "report warning and continue" "get and do not cache" "do re-validation")
nCases=${#titles[@]}

for (( i=1; i<${nCases}+1; i++ ));
do
	printf "\n${GREEN}${i}. ${titles[$i-1]}${NC}\n"
	printf "in this test case, the proxy should ${exps[$i-1]}\n\n"
	sleep 1
	printf "$(cat testCases/request${i}.txt)"
	printf "\n\ntimeout 5 nc localhost 1234 < testCases/request${i}.txt > /dev/null 2>&1\n\n"
	timeout 5 nc localhost 1234 < testCases/request${i}.txt > /dev/null 2>&1
done



printf "\n${GREEN}9. Expire Example${NC}\n\n"
printf "First build a fake response for request 1 (see above) with max-age=0
you can check this in src/testCases/response_1.txt
copy this to our cache 'cp ./testCases/response_1.txt ./__cache__/response_1'
then redo request 1
WARNING: this test may fail because we're testing in a tricky way that relies on
 the id of a request, which is not guaranteed to be 1"
cp ./testCases/response_1.txt ./__cache__/response_1
sleep 1
timeout 5 nc localhost 1234 < testCases/request1.txt > /dev/null 2>&1

# ------------------------------------- END TEST CASES ---------------------------------------














printf "\n\n\n"
printf "######################################################################
########################## ${GREEN}End Testing${NC} ###############################
######################################################################\n\n"
printf "killing demo process with pid $demoPid\n\n"
kill "$demoPid"


printf "######################################################################
##################### ${GREEN}Start Proxy As Daemon${NC} ##########################
######################################################################\n\n"

echo 'run ./main\n'
./main


printf "######################################################################
######################## ${GREEN}Proxy Is Running${NC} ############################
######################################################################\n\n"

echo 'shell script goes to a while(1) loop...'

while true ; do continue ; done