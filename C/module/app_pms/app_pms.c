#ifdef GRANDSTREAM_NETWORKS
/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2016-2017, chaozhen deng <chzhdeng@grandstream.cn>
 *
 * app_fastagi.c adapted from the pms
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*** MODULEINFO
	<depend>sqlite3</depend>
	<support_level>core</support_level>
 ***/

/***
	PMS(arg1,arg2,arg3)
		arg1: wake
		arg2: 0--cancel wakeup, 1--set wakeup, 2--executed wakeup
		arg3: 1--answer, 2--no response, 3--busy, 4--error

		arg1: st
		arg2: 1--checkin, 2--cleaning, 3--maintenance
		arg3: 0--Meaningless

***/

#define AST_MODULE_LOG "pms"

#include "asterisk.h"

#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/un.h>

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 328401 $")

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/channel.h"
#include "asterisk/dsp.h"

typedef sqlite3* db_conn;

static char *app = "PMS";

#define PMS_BUF_MIN_LEN    16
#define PMS_EXTEN_LEN       64
#define PMS_BUF_MAX_LEN    256

struct pms_task{
	char type[PMS_BUF_MIN_LEN];
	char status[PMS_EXTEN_LEN];
	char opt[PMS_BUF_MIN_LEN];
	char extension[PMS_EXTEN_LEN];
	db_conn connection;
};

#define PMS_SIZE sizeof(struct pms_task)

#define WAKE_CANCEL_STATUS     "0"
#define WAKE_SET_STATUS          "1"
#define WAKE_EXECUTE_STATUS    "2"
#define PMS_SET_SUCCESS    "set-success"
#define PMS_CAN_NOT_SET    "can-not-set"


#define ast_play_voice_prompt(chan, content) ast_stream_and_wait(chan, content, AST_DIGIT_ANY)

#define clean_pms_task(pms) \
	if (pms){ \
		free(pms); \
	} \

#define AST_GET_PMS_ARGS(a, b, c) \
	if (!ast_strlen_zero(c)) { \
		strncpy(b, c, sizeof(b)-1); \
	}else{ \
		clean_pms_task(a); \
		return -1; \
	} \

int g_pms_ha_enable = 0;

static int connect_db(struct pms_task *pms)
{
	int erropen;

	if (pms == NULL){
		return -1;
	}

	erropen = sqlite3_open(UCM_CONF_DB, &(pms->connection));
	if(SQLITE_OK != erropen){
		ast_log(LOG_WARNING, "connect db failed!\n");
		sqlite3_close(pms->connection);
		return -1;
	}

	return 0;
}

static void disconnect_db(struct pms_task *pms)
{
	sqlite3_close(pms->connection);
}

static int pms_get_haon(void)
{
    FILE *ha_fp = NULL;
    char on_buf[64] = { 0 };

    /* Get the ha_enable */
    ha_fp = popen("nvram get haon", "r");
    if (ha_fp != NULL)
    {
        while (ha_fp != NULL && (!feof(ha_fp)) && (fgets(on_buf, sizeof(on_buf) - 1, ha_fp) != NULL))
        {
            ast_log(LOG_DEBUG, "haon Read: [%s].", on_buf);
        }
        pclose(ha_fp);
    }
    g_pms_ha_enable = atoi(on_buf);

    ast_log(LOG_DEBUG, "ha_enable is %d.", g_pms_ha_enable);
    return g_pms_ha_enable;

}

static void pms_send_sql2ha(const char *sql)
{
    int connect_fd = -1;
    struct sockaddr_un srv_addr;

    /*if ha is on, write it to file*/
    if (NULL == sql || 1 != g_pms_ha_enable)
    {
        ast_log(LOG_DEBUG, "sql is null or ha is not enable." );
        return;
    }

    ast_log(LOG_DEBUG, "pms_send_sql2ha send buf is [%s] !", sql);

    /* remove the socket have the same name */
    connect_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if ( 0 >= connect_fd)
    {
        ast_log(LOG_ERROR, "pms_send_sql2ha creat socket fail: %d!", connect_fd);
        return;
    }
    /* set  server addr_param */
    srv_addr.sun_family = AF_UNIX;
    strncpy(srv_addr.sun_path, "/data/HA/sql.socket", sizeof(srv_addr.sun_path) - 1);

    /* bind sockfd and addr */
    if(-1 == connect(connect_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)))
    {
        close(connect_fd);
        return;
    }
    if (!send(connect_fd, sql, strlen(sql)+1, 0))
    {
        ast_log(LOG_ERROR, "pms_send_sql2ha send msg error, err_code is: %d!", connect_fd);
    }

    close(connect_fd);
}

static int update_pms_db(const char *sql, db_conn connection)
{
	int err = 0;
	int connect_busy = 0;
	char *szErrMsg = 0;
//    char sys_cmd[256] = {0};

	//sqlite3_busy_timeout(connection, 5000);
	do {
		err = sqlite3_exec(connection,"BEGIN IMMEDIATE", NULL,NULL, &szErrMsg);
		if(SQLITE_OK == err){
			connect_busy = 0;
		}else if(SQLITE_BUSY == err){
			connect_busy = 1;
			/**sleep 0.5s**/
			usleep(500000);
		}else{
			ast_log(LOG_WARNING, "begin error!\n");
			sqlite3_free(szErrMsg);
			return -1;
		}
	}while(connect_busy);

	ast_log(LOG_VERBOSE, "sql command: %s \n", sql);
	err = sqlite3_exec(connection, sql, NULL, NULL, &szErrMsg);
	if(SQLITE_OK != err){
		ast_log(LOG_WARNING, "Update failed!\n");
		sqlite3_free(szErrMsg);
		return -1;
	}

	err = sqlite3_exec(connection,"COMMIT", NULL,NULL, &szErrMsg);
	if(SQLITE_OK != err){
		ast_log(LOG_WARNING, "commit error!\n");
		sqlite3_exec(connection,"ROLLBACK", NULL,NULL, &szErrMsg);
		sqlite3_free(szErrMsg);
		return -1;
	}

    pms_send_sql2ha(sql);

#if 0
    if (1 == g_pms_ha_enable)
    {
        snprintf(sys_cmd, sizeof(sys_cmd)-1, "echo \"%s\" > /data/HA/fc_wakeup.sql", sql);
        ast_log(LOG_DEBUG, "sys_cmd is %s!\n", sys_cmd);
        system(sys_cmd);
    }
#endif

	return 0;
}

static int update_wakeup_execute_status(struct ast_channel *chan, struct pms_task *pms)
{
	int ret = 0;
	char cmd[PMS_BUF_MAX_LEN] = {0};

	if (0 == strcmp(pms->status, WAKE_CANCEL_STATUS) ||0 == strcmp(pms->status, WAKE_SET_STATUS)){
		snprintf(cmd, sizeof(cmd)-1, "UPDATE pms_wakeup SET w_action = '%s',send_status = 1 WHERE address IN "
									"(SELECT address FROM pms_room WHERE extension = '%s'); ", pms->status, pms->extension);
	}else if(0 == strcmp(pms->status, WAKE_EXECUTE_STATUS)){
		snprintf(cmd, sizeof(cmd)-1, "UPDATE pms_wakeup SET w_action = '%s', send_status = 1, w_status = '%s' WHERE address IN "
									"(SELECT address FROM pms_room WHERE extension = '%s'); ", pms->status, pms->opt, pms->extension);
	}

	manager_event(EVENT_FLAG_CALL, "PMSWakeupStatus",
		"Extension: %s\r\n"
		"WakeupActionStatus: %s\r\n"
		"WakeupAnswerStatus: %s\r\n",
		pms->extension?pms->extension:"",
		pms->status?pms->status:"",
		pms->opt?pms->opt:"");

	ret = update_pms_db(cmd, pms->connection);

	return ret;
}

static int update_st_status(struct ast_channel *chan, struct pms_task *pms)
{
	int ret = 0;
	char cmd[PMS_BUF_MAX_LEN] = {0};
	const char *maid = NULL;
	time_t timer;
	char s_date[PMS_BUF_MIN_LEN] = {0};
	char s_time[PMS_BUF_MIN_LEN] = {0};
	const char *report_status = NULL;
	int flag_minibar = 0;

	time( &timer);
	strftime(s_time, sizeof(s_time)-1, "%H%M", localtime(&timer));
	strftime(s_date, sizeof(s_date)-1, "%Y%m%d", localtime(&timer));

	report_status = pbx_builtin_getvar_helper(chan, "MINIREPROTSTATUS");
	if (report_status != NULL && strcasecmp(report_status, "yes") == 0){
		flag_minibar = 1;
	}
	maid = pbx_builtin_getvar_helper(chan, "MAID");
	snprintf(cmd, sizeof(cmd)-1, "UPDATE pms_room SET status = '%s', s_date = '%s', s_time = '%s', maid = '%s', send_status = 1 WHERE extension = '%s';", \
								pms->status, s_date, s_time, maid?maid:"0000", pms->extension);
	ret = update_pms_db(cmd, pms->connection);
	if (flag_minibar == 0){
		if(ret == 0){
			ast_play_voice_prompt(chan, PMS_SET_SUCCESS);
		}else{
			ast_play_voice_prompt(chan, PMS_CAN_NOT_SET);
		}
	}else{
		if(ret != 0){
			pbx_builtin_setvar_helper(chan, "MINIREPROTSTATUS", "no");
		}
	}

	manager_event(EVENT_FLAG_CALL, "PMSRoomStatus",
		"Extension: %s\r\n"
		"RoomActionStatus: %s\r\n",
		pms->extension?pms->extension:"",
		pms->status?pms->status:"");

	return ret;
}

/*** update ucm wakeup execute status ***/
static int update_ucm_wakeup_execute_status(struct ast_channel *chan, struct pms_task *wakeup)
{
	int ret = 0;
	char cmd[256] = {0};
	const char *execute_date = NULL;
	const char *wakeup_index = NULL;

	execute_date = pbx_builtin_getvar_helper(chan, "UCM_WAKEUP_DATE");
	wakeup_index = pbx_builtin_getvar_helper(chan, "WAKEUP_INDEX");

	if (execute_date == NULL || wakeup_index == NULL  || wakeup == NULL)
	{
	    return -1;
	}

	snprintf(cmd, sizeof(cmd)-1, "update ucm_wakeup_members set execute_status='%s',answer_status='%s', execute_date='%s' "
								"where member_extension='%s' and wakeup_index = '%s';",
								wakeup->status, wakeup->opt, execute_date, wakeup->extension, wakeup_index);
	//ast_debug(1, "cmd: %s", cmd);
	ret = update_pms_db(cmd, wakeup->connection);

	return ret;
}

static struct pms_task *create_pms_task(void)
{
	struct pms_task *tmp_task = NULL;
	tmp_task = (struct pms_task *)malloc(PMS_SIZE);
	if (tmp_task){
		memset(tmp_task, 0, PMS_SIZE);
		return tmp_task;
	}else{
		return NULL;
	}
}

static int pms_exec(struct ast_channel *chan, const char *data)
{
	int ret = 0;
	char *parse = NULL;
	const char *exten = NULL;
	struct pms_task *p_task = NULL;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(pms_type);
		AST_APP_ARG(pms_status);
		AST_APP_ARG(pms_opt);
	);

	ast_debug(1, "enter the apply of pms status settings! \n");
	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "This application requires at least one argument (destination(s) to pms status settings)\n");
		return -1;
	}

	p_task = create_pms_task();
	if (p_task == NULL){
		ast_log(LOG_WARNING, "create task fail!");
		return -1;
	}

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);
	AST_GET_PMS_ARGS(p_task, p_task->type, args.pms_type);
	AST_GET_PMS_ARGS(p_task, p_task->status, args.pms_status);
	AST_GET_PMS_ARGS(p_task, p_task->opt, args.pms_opt);
	exten = pbx_builtin_getvar_helper(chan, "TARGET");
	AST_GET_PMS_ARGS(p_task, p_task->extension, exten);
	ast_debug(1, "type: %s, status: %s, opt: %s, extension: %s ",
				p_task->type, p_task->status, p_task->opt, p_task->extension);

	if (connect_db(p_task)){
		clean_pms_task(p_task);
		return -1;
	}

	if(0 == strcmp(p_task->type, "wake")){
		ret = update_wakeup_execute_status(chan, p_task);
	}else if(0 == strcmp(p_task->type, "st")){
		ret = update_st_status(chan, p_task);
	}else if(0 == strcmp(p_task->type, "ucmWake")){
		ret = update_ucm_wakeup_execute_status(chan, p_task);
	}

	if(0 != ret){
		ast_log(LOG_WARNING, "update extension[%s] information fail!", p_task->extension);
	}

	disconnect_db(p_task);
	clean_pms_task(p_task);

	ast_debug(1, "end the apply of pms status settings! \n");
	return 0;
}

static int unload_module(void)
{
	int res;
	res = ast_unregister_application(app);
	return res;
}

static int load_module(void)
{
	int res;

    /*Get HA status*/
    pms_get_haon();
	res = ast_register_application_xml(app, pms_exec);
	return res;
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Trivial PMS Status SettingsApplication");

#endif
