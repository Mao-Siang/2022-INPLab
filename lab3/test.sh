#!/bin/sh
timeout 20 ./tcpcbr 1;   sleep 5
timeout 20 ./tcpcbr 1.5; sleep 5
timeout 20 ./tcpcbr 2;   sleep 5
timeout 20 ./tcpcbr 3