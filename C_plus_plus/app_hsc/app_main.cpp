#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "pms_global.h"
#include "pms_hmobile.h"
#include "pms_mitel.h"
#include "app_hsc.h"

#define PMS_NEW(type) (new  type)
#define PMS_SAFE_DELETE( obj ) if ( obj != NULL ) { delete obj; obj = NULL;}

using namespace pmsBaseNamespace;
using namespace pmsHMBaseNamespace;
using namespace pmsMitelBaseNamespace;
using namespace pmsHscBaseNamespace;

static pthread_t pms_phread;
static int is_pms_running = 0;

int pmsStatusGet()
{
    return is_pms_running;
}

static void pmsStatusSet(int status)
{
    if (0==status || 1==status)
    {
        is_pms_running = status;
    }
}

static void pmsDataBaseConnect()
{
#if 0
    if (g_connection[PBXMID_UCM_CONFIG_DB] == NULL)
    {
        connectConfigDB(PBXMID_UCM_CONFIG_DB);
    }

    if (g_connection[PBXMID_UCM_CONFIG_DB] == NULL)
    {
        connectConfigDB(PBXMID_UCM_CONFIG_DB);
    }

    if (g_connection[DB_AST_EVENT_PMS] == NULL)
    {
       connectConfigDB(DB_AST_EVENT_PMS);
    }
#endif

    cgilog(SYSLOG_DEBUG, "pmsDataBaseConnect =========");
}

static void pmsDataBaseDisconnect()
{
#if 0
    disconnectDB(PBXMID_UCM_CONFIG_DB);
    cgilog(SYSLOG_DEBUG, "Disconnect pms server database!");

    disconnectDB(PBXMID_UCM_CONFIG_DB);
    cgilog(SYSLOG_DEBUG, "Disconnect pms client database !");

    disconnectDB(PBXMID_ASTERISK_EVENT_DB);
    cgilog(SYSLOG_DEBUG, "Disconnect pms event database !");
#endif
}

static pmsBase *pmsModuleCreate()
{
    char *pms_protocol = NULL;
    pmsBase *pmsEvent = NULL;
    char *cmd = "SELECT pms_protocol from pms_settings;";

    pmsDataBaseConnect();

    pms_protocol = runSQLGetOneVarcharField(cmd, "pms_protocol", PBXMID_UCM_CONFIG_DB);
    if (NULL != pms_protocol)
    {
        cgilog(SYSLOG_DEBUG, "pmsModuleCreate : %s", pms_protocol);
        if (strcasecmp(pms_protocol, "hmobile") == 0)
        {
            pmsEvent = PMS_NEW(pmsHmobileBase);
        }
        else if(strcasecmp(pms_protocol, "mitel") == 0)
        {
            pmsEvent = PMS_NEW(pmsMitelBase);
        }
        else if(strcasecmp(pms_protocol, "hsc") == 0)
        {
            pmsEvent = PMS_NEW(pmsHscBase);
        }
    }
    else
    {
        cgilog(SYSLOG_DEBUG, "pmsModuleCreate : pms_protocol is NULL!!!");
        pmsDataBaseDisconnect();
    }

    SAFE_FREE(pms_protocol);

    return pmsEvent;
}

static void pmsModuleDestroy(pmsBase *pmsEvent)
{
    PMS_SAFE_DELETE(pmsEvent);
    cgilog(SYSLOG_DEBUG, "pmsModuleDestroy");

    pmsDataBaseDisconnect();
}

static void *pmsModuleThread(void *arg)
{
    pmsBase *pmsEvent = (pmsBase *)(arg);
    cgilog(SYSLOG_DEBUG, "----Enter '__%s__' function----!", __FUNCTION__);

    /*init pms */
    if (MIDCODE_SUCCESS != pmsEvent->pmsConnectInit())
    {
        cgilog(SYSLOG_ERR, "----- Init pms fail. -----");
        pthread_detach(pthread_self());
        pmsStatusSet(0);
    }

    connectConfigDB(PBXMID_UCM_CONFIG_DB, __FUNCTION__);
    connectConfigDB(PBXMID_AMI_STATUS_DB, __FUNCTION__);
    connectConfigDB(PBXMID_ASTERISK_EVENT_DB, __FUNCTION__);

    pmsEvent->pmsConnectLogin();

    while (is_pms_running)
    {
        /*handle task as server*/
        pmsEvent->pmsServerHandler();

        /*handle task queue*/
        if (true == pmsEvent->pmsGetClientModeSupport())
        {
            pmsEvent->pmsQueueHandler();
        }

        /*handle task as client*/
        if (true == pmsEvent->pmsGetClientModeSupport())
        {
            pmsEvent->pmsClientHandler();
        }

    }
    if (true == pmsEvent->pmsGetClientModeSupport())
    {
        pmsEvent->pmsClientTaskClean();
    }

    pmsEvent->pmsConnectLogout();
    pmsEvent->pmsConnectExit();

    disconnectDB(PBXMID_UCM_CONFIG_DB);
    disconnectDB(PBXMID_AMI_STATUS_DB);
    disconnectDB(PBXMID_ASTERISK_EVENT_DB);

    pmsModuleDestroy(pmsEvent);

    pthread_exit(NULL);

    return NULL;
}

void pmsStart()
{
    int enable = 0;
    pthread_attr_t pms_monitor;
    pmsBase *pmsEvent = NULL;

    if (checkPMSEnabledNvram() == 0)
    {
        enable = 0;
    }
    else if (checkPMSEnabledNvram() == 1)
    {
        enable = 1;
    }
    else
    {
        setNvram(PVALUE_PMS_ENABLE, "1");
        enable = 1;
    }

    if (enable)
    {
        pmsEvent = pmsModuleCreate();
        if (pmsEvent)
        {
            pmsStatusSet(1);
            pthread_attr_init(&pms_monitor);
            pthread_attr_setstacksize((&pms_monitor), PMS_STACK_SIZE);
            pthread_attr_setdetachstate((&pms_monitor), PTHREAD_CREATE_JOINABLE);
            pthread_create(&pms_phread, &pms_monitor, (void* (*)(void*))pmsModuleThread, pmsEvent);
        }
        if(pmsGetHAOn())
        {
            pmsGetPeerHAIP();
            pmsGetPeerHAPort();
            pmsGetHAOnline();
            pmsGetHARole();
            setNvramNotification(PVALUE_HA_ROLE);
        }

    }
}

void pmsStop()
{
    if (pmsStatusGet())
    {
        pmsStatusSet(0);
        pthread_join(pms_phread, NULL);
    }
}

