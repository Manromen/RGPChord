#!/bin/bash

# start a second interactive chord node
echo 'starting a seconds interactive chord node...'
./chord -ip 127.0.0.1 -port 2001 -cip 127.0.0.1 -cport 2000
