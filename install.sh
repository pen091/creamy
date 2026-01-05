#!/bin/bash

gcc server.c -o server -pthread
gcc client.c -o client -pthread
gcc stress_test.c -o stress_test -pthread


