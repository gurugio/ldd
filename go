#!/bin/sh
make
scp -P 7777 my_brd.ko gurugio@localhost:/home/gurugio/my_brd.ko
