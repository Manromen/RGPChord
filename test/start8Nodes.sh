#!/bin/bash

# 1
nohup ./chord -daemon -v -ip 127.0.0.1 -port 2014 -cip 127.0.0.1 -cport 2000 > node2014.log 2> node2014.err &

# 2
sleep 0.2 # node id is Chosen by random number - we need another seed (depending on current time) -> so we need to sleep a bit
nohup ./chord -daemon -v -ip 127.0.0.1 -port 2015 -cip 127.0.0.1 -cport 2000 > node2015.log 2> node2015.err &

# 3
sleep 0.2
nohup ./chord -daemon -v -ip 127.0.0.1 -port 2016 -cip 127.0.0.1 -cport 2000 > node2016.log 2> node2016.err &

# 4
sleep 0.2
nohup ./chord -daemon -v -ip 127.0.0.1 -port 2017 -cip 127.0.0.1 -cport 2000 > node2017.log 2> node2017.err &

# wait a bit to give nodes with port 2014 and 2015 time to wait for incomming connections
sleep 1

# 5
nohup ./chord -daemon -v -ip 127.0.0.1 -port 2018 -cip 127.0.0.1 -cport 2014 > node2018.log 2> node2018.err &

# 6
sleep 0.2
nohup ./chord -daemon -v -ip 127.0.0.1 -port 2019 -cip 127.0.0.1 -cport 2014 > node2019.log 2> node2019.err &

# 7
sleep 0.2
nohup ./chord -daemon -v -ip 127.0.0.1 -port 2020 -cip 127.0.0.1 -cport 2015 > node2020.log 2> node2020.err &

# 8
sleep 0.2
nohup ./chord -daemon -v -ip 127.0.0.1 -port 2021 -cip 127.0.0.1 -cport 2015 > node2021.log 2> node2021.err &
