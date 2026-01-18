#!/bin/sh


if [ $# -ne 0  ]; then
    if [ "$1" -eq 1 ]; then
        cp config.txt ./hidden/backup/config.txt
        cp ./hidden/config.txt config.txt
        mkdir -p /tmp/up_hw1_hidden/ && cp -R ./hidden/backup/dir* /tmp/up_hw1_hidden/
        ./logger config.txt ./hidden/hidden_1
        rm -rf /tmp/up_hw1_hidden/
        cp ./hidden/backup/config.txt config.txt
    elif [ "$1" -eq 2 ]; then
        cp config.txt ./hidden/backup/config.txt
        cp ./hidden/config.txt config.txt
        ./logger config.txt ./hidden/hidden_2 www.yahoo.com www.github.com 8.8.8.8 1.1.1.1
        cp ./hidden/backup/config.txt config.txt
    elif [ "$1" -eq 3 ]; then
        cp config.txt ./hidden/backup/config.txt
        cp ./hidden/config.txt config.txt
        mkdir -p /tmp/up_hw1_hidden/dir3 && cp -R ./hidden/backup/dir* /tmp/up_hw1_hidden/
        ln -s /tmp/up_hw1_hidden/dir1/file_1.txt /tmp/up_hw1_hidden/dir3/file_1.txt
        ln -s /tmp/up_hw1_hidden/dir2/file_1.txt /tmp/up_hw1_hidden/dir3/file_2.txt
        ./logger config.txt ./hidden/hidden_3 
        rm -rf /tmp/up_hw1_hidden/
        cp ./hidden/backup/config.txt config.txt
    elif [ "$1" -eq 4 ]; then
        cp config.txt ./hidden/backup/config.txt
        cp ./hidden/config.txt config.txt
        ./logger config.txt ./hidden/hidden_4
        cp ./hidden/backup/config.txt config.txt
    fi
else
    echo "Usage: \"./test_hidden.sh [case num]\"";
    exit
fi