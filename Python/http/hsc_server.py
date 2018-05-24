#!/usr/bin/env python
# coding=utf-8
# emacs: -*- mode: python; py-indent-offset: 4; indent-tabs-mode: t -*-
# vi: set ft=python sts=4 ts=4 sw=4 et :

import time
import socket
import commands

# Address
HOST = ''
PORT = 10240

DICT_MSG = {}

REQUEST_CONTENT = ''
RESPONSE_CONTENT = ''

def do_command(cmd):
    (status, output) = commands.getstatusoutput(cmd)
    if status == 0:
        return output

def addHttpHeaderItem(item):
    global RESPONSE_CONTENT
    if item != '\r\n':
        item = '%s\r\n'%(item)

    if len(RESPONSE_CONTENT) == 0:
        RESPONSE_CONTENT = '%s'%(item)
    else:
        RESPONSE_CONTENT = '%s%s'%(RESPONSE_CONTENT,item)

def addHttpBodyItem(item):
    global RESPONSE_CONTENT
    if len(RESPONSE_CONTENT) == 0:
        RESPONSE_CONTENT = '%s'%(item)
    else:
        RESPONSE_CONTENT = '%s%s'%(RESPONSE_CONTENT,item)

def addMessage(method):
    global DICT_MSG

    addHttpHeaderItem('HTTP/1.0 200 OK')

    date = time.strftime("Date: %a, %b %d %H:%M:%S %Y GMT", time.localtime())
    addHttpHeaderItem(date)

    addHttpHeaderItem('Server: Apache')
    addHttpHeaderItem('Connection: close')
    addHttpHeaderItem('Content-Type: application/json')

    if method == 'POST':
        content = str(DICT_MSG)
        con_len = len(content)
        hdr_item = 'Content-Length: %d'%(con_len)
        addHttpHeaderItem(hdr_item)
        addHttpHeaderItem('\r\n')

        addHttpBodyItem(content)
    else:
        addHttpHeaderItem('Content-Length: 0')
        addHttpHeaderItem('\r\n')

def getRequestHeader(src):
    message = src.split('\r\n\r\n')
    if len(message) > 0:
        tmp = message[0]
    else:
        tmp = ''

    return tmp

def getRequestBody(src):
    message = src.split('\r\n\r\n')
    if len(message) > 0:
        tmp = message[1]
    else:
        tmp = ''

    return tmp

def getRequestLen(src):
    len = 0
    header = src
    if header.find('Content-Length') >= 0:
        listHdr = header.split('\r\n')
        for item in listHdr:
            if item.find('Content-Length')>= 0:
                #print '-----%s----'%(item)
                tmp = item.split(':')
                request_len = tmp[1]
                len = int(request_len)

    return len

def getRequestMethod(src):
    tmp = src.split(' ')[0]
    print 'Method: %s'%(tmp)

    return tmp

def hscLogger(type, addr, data):
    print '<<<------------------ %s %s ------------------>>>'%(type, str(addr))
    print '%s'%(data)
    print '-------------------------------------------------'

def handleMessages(msg):
    global DICT_MSG

    print 'handle: %s'%(msg)
    dict_msg = eval(msg)
    for item in dict_msg.keys():
        DICT_MSG[item] = dict_msg[item]

def hscServer():
    global DICT_MSG
    global REQUEST_CONTENT
    global RESPONSE_CONTENT

    s    = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
    #socket.setdefaulttimeout(20)
    s.bind((HOST, PORT))
    s.listen(3)

    # Serve forever
    while True:
        conn, addr = s.accept()

        print "receive start ... ..."
        while True:
            request    = conn.recv(1024)
            REQUEST_CONTENT = '%s%s'%(REQUEST_CONTENT,request)

            method     = getRequestMethod(REQUEST_CONTENT)
            if method == 'POST':
                header  = getRequestHeader(REQUEST_CONTENT)
                body    = getRequestBody(REQUEST_CONTENT)
                req_len = getRequestLen(header)
                rcv_len = len(body)
                print 'request len: %d, receive len: %d'%(req_len, rcv_len)
                if req_len == rcv_len and req_len != 0:
                    break
            else:
                break

        hscLogger('request message from ', addr, REQUEST_CONTENT)
        # if GET method request
        if method == 'GET':
            # send message
            addMessage(method)
        # if POST method request
        elif method == 'POST':
            body = getRequestBody(REQUEST_CONTENT)
            handleMessages(body)
            addMessage(method)

        hscLogger('response message to', addr, RESPONSE_CONTENT)
        conn.sendall(RESPONSE_CONTENT)

        # close connection
        DICT_MSG = {}
        REQUEST_CONTENT = ''
        RESPONSE_CONTENT = ''
        conn.close()

# ---------- main -------------
hscServer()