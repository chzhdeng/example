#!/usr/bin/env python
# coding=utf-8

import sqlite3
import os
import commands

DB_PATH="/cfg/etc/ucm_config.db"

def err_end (err_id):
    sys.stdout.write(str(err_id))
    exit(1)

def do_command (cmd):
    (status, output) = commands.getstatusoutput(cmd)
    if status != 0:
        err_end(-9)
    return output

def addWakeupCount ():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    c = conn.cursor()

    i=1
    while i < 2000  :
        sql = "INSERT INTO ucm_wakeup VALUES(%d,'first',1,'1910','wakeup-call','0,1,2,3,4,5,6','10:00',NULL);"%(i)
        c.execute(sql)
        i=i+1

    conn.commit()
    conn.close()

#---- main ----
addWakeupCount()
