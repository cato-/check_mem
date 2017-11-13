#
# Regular cron jobs for the checkmem package
#
0 4	* * *	root	[ -x /usr/bin/checkmem_maintenance ] && /usr/bin/checkmem_maintenance
