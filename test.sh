#! /bin/bash

YELLOW="\033[1;33m"
NC="\033[0m"

gcc ./test/test_get.c  -o get
gcc ./test/test_ctl.c  -o ctl -pthread
gcc ./test/test_send_recv.c  -o send_recv -pthread

clear

echo -e "\n\n${YELLOW}*** testing tag_get ***${NC}\n"
./get
echo -e "\n\n${YELLOW}*** testing tag_ctl ***${NC}\n"
./ctl
echo -e "\n\n${YELLOW}*** testing tag_send and tag_receive ***${NC}\n"
./send_recv

rm get
rm ctl
rm send_recv