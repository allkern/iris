#!/bin/bash

cd gl3w;
./gl3w_gen.py;
rm src/glfw_test.c src/glut_test.c
