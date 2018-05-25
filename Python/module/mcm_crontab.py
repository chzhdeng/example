#!/usr/bin/env python
# coding=utf-8
# emacs: -*- mode: python; py-indent-offset: 4; indent-tabs-mode: t -*-
# vi: set ft=python sts=4 ts=4 sw=4 et :
#     *　　      *　　   *　　*　　     *　　   command
#     minute　   hour　  day　month　   week　  command

'''
Created on 2018-01-20

@author: chzhdeng
'''
import sys
import utils
from pysyslog import *
import commands
import os
import time
import sqlite3
import string

DB_NAME='/cfg/etc/ucm_config.db'
EXE_MCM_SCRIPT = "source /app/asterisk/python2.7/setup_python_env.sh && python /cfg/var/lib/asterisk/scripts/exe_mcm_reservation.py  "
MCM_CRONTAB_CONTENT=''
MCM_ARGS='--type ACTION --id BOOK-ID --conf CONF-NUM'
ACT_DICT={"playback":"PLAYY-TIME","kick":"KICK-TIME","call":"CALL-TIME","clean":"CLEAN-TIME"}

def do_command(cmd):
    (status, output) = commands.getstatusoutput(cmd)
    if status == 0:
        return output

def build_single_reservation(args,action_dict):
    global MCM_CRONTAB_CONTENT
    now_time_str = time.strftime("%Y%m%d%H%M", time.localtime())

    now_time = string.atoi(now_time_str)
    book_time = string.atoi(action_dict['call'].replace('-','').replace(':','').replace(' ',''))
    # print type(now_time)
    # print type(book_time)

    if (book_time < now_time):
        log_debug("it doesn't need to add reservation time [%s] for mcm. "%(action_dict['call']))
    else:
        for act in action_dict:
            mcm_arg = args.replace('ACTION', act)

            tmp = time.strptime(action_dict[act], '%Y-%m-%d %H:%M')
            year = time.strftime('%Y', tmp)
            month = time.strftime('%m', tmp)
            day = time.strftime('%d', tmp)
            hour = time.strftime('%H', tmp)
            minute = time.strftime('%M', tmp)

            script = "%s %s\n" % (EXE_MCM_SCRIPT, mcm_arg)
            cmd = '%s %s %s %s %s %s' % (minute, hour, day, month, year, script)
            log_debug ('cmd is %s.' % cmd)
            MCM_CRONTAB_CONTENT = "%s%s"%(MCM_CRONTAB_CONTENT, cmd)

SQL_RESERVATION_MCM = "SELECT * FROM multimedia_conference_reservation;"
def build_crontab_mcm():
    for row in c.execute(SQL_RESERVATION_MCM):
        mcm_args = MCM_ARGS
        action_dict = ACT_DICT
        book_id = utils.str_sor(row['reservation_id'])
        if book_id == "":
            continue

        conf_num = utils.str_sor(row['conf_number'])
        mcm_args = mcm_args.replace('BOOK-ID', book_id).replace('CONF-NUM', conf_num)

        start_time = utils.str_sor(row['start_time'])
        end_time = utils.str_sor(row['end_time'])
        kill_time = utils.str_sor(row['kick_time'])

        tmp_start = time.strptime(start_time, '%Y-%m-%d %H:%M')
        tmp_plackback = time.localtime(time.mktime(tmp_start) - (int(kill_time))*60)
        tmp_kick = time.localtime(time.mktime(tmp_start) - (int(kill_time) - 5)*60)
        plackback_time = time.strftime('%Y-%m-%d %H:%M', tmp_plackback)
        kill_time = time.strftime('%Y-%m-%d %H:%M', tmp_kick)

        # print plackback_time
        # print kill_time
        # print start_time
        # print end_time

        action_dict['playback']=plackback_time
        action_dict['kick']=kill_time
        action_dict['call']=start_time
        action_dict['clean']=end_time

        #cycle = utils.str_sor(row['cycle'])
        cycle = 'none'
        if cycle == 'none':
            build_single_reservation(mcm_args,action_dict)
        else:
            #build_cycle_reservation(mcm_args,action_dict)
            log_debug("handle cycle mcm reservation. ")

def clean_mcm_crontab():
    do_command("sed -i '/exe_mcm_reservation/d' /etc/crontabs/root")

def add_mcm_crontab(string):
    if string != '':
        cron_file = open("/etc/crontabs/root",'a')

        try:
            cron_file.write(string)
        finally:
            cron_file.close()

#----------main---------------
log_init("other_logger")
conn=sqlite3.connect(DB_NAME)
conn.row_factory = sqlite3.Row
c=conn.cursor()

clean_mcm_crontab()
build_crontab_mcm()
#print MCM_CRONTAB_CONTENT
add_mcm_crontab(MCM_CRONTAB_CONTENT)

conn.close()
