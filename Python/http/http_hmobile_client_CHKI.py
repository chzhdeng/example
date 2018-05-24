#!/usr/bin/env python
# -*- coding: utf-8 -*-

'''
Created on 2016-06-25
@author: deng chao zhen
the function of script is used for sending CHKI message.
if you want to use the script to test PMS,please modify the values of the following three parameters:

START_ADDRESS : room start address
END_ADDRESS：room  end address
SERVER_URL: URL of server, for example :192168.124.191:8081
'''

import httplib
import urllib
import time

START_ADDRESS = 1290
END_ADDRESS = 1294
SERVER_URL = '192.168.124.29:8081'

def sendhttp(address, room):
    contentText = '''
                            <?xml version="1.0"?>
                            <SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/">
                                <SOAP-ENV:Body xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" encodingStyle="http://schemas.xmlsoap.org/soap/encoding/" xmlns:NS2="http://www.hmobile.es/hmlistener" xmlns:NS3="http://www.w3.org/2001/XMLSchema" xmlns:NS4="http://www.w3.org/2001/XMLSchema" xmlns:NS5="http://www.w3.org/2001/XMLSchema" xmlns:NS6="http://www.w3.org/2001/XMLSchema" xmlns:NS7="http://www.w3.org/2001/XMLSchema" xmlns:NS8="http://www.w3.org/2001/XMLSchema" xmlns:NS9="http://www.w3.org/2001/XMLSchema">
                                    <NS1:executeActionRequest xmlns:NS1="http://www.hmobile.es/hmlistener" xmlns:NS1="http://www.hmobile.es/hmlistener">
                                        <credential type="NS2:Tcredential">
                                            <username type="NS3:string">userid</username>
                                            <requesttime type="NS4:string">20160625151625</requesttime>
                                            <password type="NS5:string">93FB37E41745295AC3A33EECB89DB4A0</password>
                                        </credential>
                                        <site type="NS6:string">hotelid</site>
                                        <address type="NS7:string">REP_ADDRESS</address>
                                        <action type="NS8:string">CHKI</action>
                                        <data type="NS9:string">&lt;CHKI&gt;&#13;
                            &lt;room&gt;REP_ROOM&lt;/room&gt;&#13;
                            &lt;account&gt;123456&lt;/account&gt;&#13;
                            &lt;name&gt;John&lt;/name&gt;&#13;
                            &lt;surname&gt;shsaeh&lt;/surname&gt;&#13;
                            &lt;language&gt;zh&lt;/language&gt;&#13;
                            &lt;vipcode&gt;2&lt;/vipcode&gt;&#13;
                            &lt;datein&gt;20100101&lt;/datein&gt;&#13;
                            &lt;dateout&gt;20100107&lt;/dateout&gt;&#13;
                            &lt;credit&gt;9999900&lt;/credit&gt;&#13;
                            &lt;cos&gt;3&lt;/cos&gt;&#13;
                            &lt;/CHKI&gt;</data>
                                    </NS1:executeActionRequest>
                                </SOAP-ENV:Body>
                            </SOAP-ENV:Envelope>
'''

    str_addr = str(address)
    str_room = str(room)
    contentText = contentText.replace('REP_ADDRESS',str_addr).replace('REP_ROOM',str_room)
    
    # Step 1: http connect
    conn = httplib.HTTPConnection(SERVER_URL)
    headers = {'Connection':'Close',
                        'Content-Type': 'text/xml; charset=utf-8',
                        'SOAPAction': 'http://www.hmobile.es/hmlistener#executeActionRequest',
                        'User-Agent': 'HMlistener'
                        }
    headers['Content-Length'] = '%d' % len(contentText)

    # Step 2: http request
    conn.request('POST', '/', contentText, headers)

    # Step 3: http recerve reponse
    httpres = conn.getresponse()
    print httpres.status
    print httpres.reason
    print httpres.read()
    print '==========end send CHKI counts:%d============='%(address -START_ADDRESS  +1)

if __name__ == '__main__':
    address = START_ADDRESS
    while address <= END_ADDRESS:
        sendhttp(address, address)
        time.sleep(0.1)
        address = address+1
