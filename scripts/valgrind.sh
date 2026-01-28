#!/bin/sh

sudo valgrind --tool=memcheck \
  --leak-check=full \
  --show-leak-kinds=all -s $@