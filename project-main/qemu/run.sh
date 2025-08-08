#!/bin/bash

qemu-system-arm -M mps2-an521 -nographic -kernel ../build/firmware.elf -s -S