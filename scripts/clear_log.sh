#!/bin/bash

export HOME=/home/$(hostname)

while [ 1 ]
do
	find $HOME/.ros/log/* -type f -mtime +8  -exec rm -rf {} \;
    find $HOME/.ros/log/unity-lans-gz/E* -type f -mtime +8  -exec rm -rf {} \;
	sleep 3600
done