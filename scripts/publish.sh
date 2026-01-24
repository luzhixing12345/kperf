#!/bin/sh

rm ../kperf_*
debuild -S -sa
file=`ls ../ | grep source.changes`
# echo $file
dput ppa:kamilu/kperf ../$file
rm ../kperf_*