#!/bin/bash

# 1
nohup ./chord -daemon -v -ip 127.0.0.1 -port 2010 -cip 127.0.0.1 -cport 2000 > node2010.log 2> node2010.err &

# 2
sleep 0.2 # node id is Chosen by random number - we need another seed (depending on current time) -> so we need to sleep a bit
nohup ./chord -daemon -v -ip 127.0.0.1 -port 2011 -cip 127.0.0.1 -cport 2000 > node2011.log 2> node2011.err &

# 3
sleep 0.2
nohup ./chord -daemon -v -ip 127.0.0.1 -port 2012 -cip 127.0.0.1 -cport 2000 > node2012.log 2> node2012.err &

# 4
sleep 0.2
nohup ./chord -daemon -v -ip 127.0.0.1 -port 2013 -cip 127.0.0.1 -cport 2000 > node2013.log 2> node2013.err &
