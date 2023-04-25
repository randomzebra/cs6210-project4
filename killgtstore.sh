#!/bin/bash
kill -9 $(ps aux | grep -e gtstore | awk '{ print $2 }') 