#!/bin/sh
bd=`TZ='America/Los_Angeles' date +"%b %d %Y"`
sed "s/Nov 10 2023/$bd/" SIMPLE.ASM > SIMPLE_TMP.ASM
