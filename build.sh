#!/bin/bash
mkdir -p bin
eval cc src/main.c -lpthread -o bin/Server
./bin/Server
