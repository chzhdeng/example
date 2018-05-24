#!/usr/bin/env python
# vim: set et sw=4:
import os
import commands
import socket
import sys
import argparse
import json

reload(sys)
sys.setdefaultencoding('utf8')

#Unix socket for new interface
SQL_MCM_SOCKET = '/tmp/mcmman.socket'

def argsExplain():
    print "Please input like:"
    print "    python /cfg/var/lib/asterisk/scripts/exe_mcm_reservation.py --type XXXX --id XXXX --conf XXX"
    print "    python /cfg/var/lib/asterisk/scripts/exe_mcm_reservation.py --help' to view help information"

def create_mcm_message(action, bookid, number, option):
    global MCM_MSG
    MCM_MSG = {}
    MCM_MSG['action'] = action
    MCM_MSG['action_id'] = "65535"
    MCM_MSG['book_id'] = bookid
    MCM_MSG['conf_number'] = number

    #add option for Reload event
    if args.conf != False:
        MCM_MSG['type'] = option

    if action == 'playback':
        MCM_MSG['file'] = "schedule-conference-kickall.gsm"
    elif action == 'kick':
        MCM_MSG['action'] = "KickUser"
        MCM_MSG['user_id'] = "all"
    elif action == 'call':
        MCM_MSG['action'] = "BookInvite"
    elif action == 'clean':
        MCM_MSG['action'] = "BookClean"

    print MCM_MSG

def handle_mcm_reservation():
    tmp_msg_body = {"type": "request","message": "MCM-MESSAGE"}
    tmp_msg_body['message'] = MCM_MSG
    msg_body = json.dumps(tmp_msg_body)
    msg_len = len(msg_body)

    msg_head = "Content-Length: %d\r\n"%msg_len

    message = "%s\r\n%s"%(msg_head, msg_body)
    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.settimeout(3)
        sock.connect(SQL_MCM_SOCKET)

        # print "Request message:"
        # print "----------------------->>>"
        # print message
        # print "----------------------->>>"
        sock.send(message)

        # print "Response message:"
        # buf = sock.recv(1024)
        # print "<<<-----------------------"
        # print buf
        # print "<<<-----------------------"

        sock.close()
    except BaseException,e:
        print e

#------------main--------------
parser = argparse.ArgumentParser()
parser.add_argument("--type", default = False, help="type value: playback, kick, call or clean")
parser.add_argument("--id", default = False, help="multimedia conference reservation id")
parser.add_argument("--conf", default = False, help="conference reservation number")
parser.add_argument("--opt", default = False, help="add option")

args = parser.parse_args()
if args.type != False and args.id != False and args.conf != False:
    create_mcm_message(args.type, args.id, args.conf, args.opt)
    handle_mcm_reservation()
else:
    argsExplain()
