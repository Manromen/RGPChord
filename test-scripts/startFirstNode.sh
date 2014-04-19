#!/bin/bash

# kill all previous started nodes
echo ''
echo 'killing all possible running chord processes...'
killall chord

# remove logs and error logs from previous session
echo ''
echo 'removing logs from possible previous session...'
rm node2010.log node2011.log node2012.log node2013.log node2014.log node2015.log node2016.log node2017.log node2018.log node2019.log node2020.log node2021.log
rm node2010.err node2011.err node2012.err node2013.err node2014.err node2015.err node2016.err node2017.err node2018.err node2019.err node2020.err node2021.err 

# creates the chord -> no daemon
echo ''
echo 'starting first chord node...'
./chord -ip 127.0.0.1 -port 2000

