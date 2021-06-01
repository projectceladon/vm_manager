#!/bin/bash
ip_address = ip -o addr show scope global | awk '$2 != "docker0"' | awk '{split($4, a, "/"); print a[1]}'
sudo ffmpeg -f v4l2 -s 640x480 -i /dev/video0 -pix_fmt yuv420p -r 30 -f rawvideo tcp://$ip_address:8085 -hide_banner -loglevel error &

