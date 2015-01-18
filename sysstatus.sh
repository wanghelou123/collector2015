#! /bin/sh
echo system boot time: `date -d "$(awk -F. '{print $1}' /proc/uptime) second ago" +"%Y-%m-%d %H:%M:%S"`
cat /proc/uptime| awk -F. '{run_days=$1 / 86400;run_hour=($1 % 86400)/3600;run_minute=($1 % 3600)/60;run_second=$1 % 60;printf("system already runing: %d day %d hour %d minute %d seconds\n",run_days,run_hour,run_minute,run_second)}'
