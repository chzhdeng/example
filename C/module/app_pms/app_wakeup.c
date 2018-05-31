#ifdef GRANDSTREAM_NETWORKS
/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2016-2017, chaozhen deng <chzhdeng@grandstream.cn>
 *
 * app_wakeup.c adapted from the pms wakeup
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

#ifdef GRANDSTREAM_NETWORKS
#define AST_MODULE_LOG "wakeup"
#endif

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

ASTERISK_FILE_VERSION(__FILE__, "$Revision: 328401 $")

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/channel.h"
#include "asterisk/dsp.h"
#include <sys/un.h>

typedef sqlite3* db_conn;

static char *app = "WakeUp";
static char *app2 = "UCMWakeUp";

int wakeup_max_num = 500;

#define WAKEUP_TYPE_OF_UCM 0
#define WAKEUP_TYPE_OF_PMS 1

#define WAKEUP_MAX_LEN  64
#define WAKEUP_MIN_LEN  12
#define WAKEUP_SIZE sizeof(struct wakeup_info)
#define  AST_PMS_WAKEUP_CRONTAB    "python /cfg/var/DB2File_PY/pms_wakeup_crontab.py"
#define  AST_UCM_WAKEUP_CRONTAB    "python /cfg/var/lib/asterisk/scripts/ucm_wakeup_crontab.py && python /cfg/var/lib/asterisk/scripts/import_ucm_wakeup.py"

#define WAKEUP_TYPE_OF_SINGLE  "1"
#define WAKEUP_TYPE_OF_DAILY   "2"

#define WAKEUP_CLOSE  "0"
#define WAKEUP_OPEN   "1"

#define WAKEUP_PLAY_SET_SUCCESS  "set-success"
#define WAKEUP_PLAY_SET_FAILTURE  "set-failture"
#define WAKEUP_PLAY_SET_INVALID    "set-invalid"
#define WAKEUP_PLAY_SET_EFFECTIVE "set-effective"
#define WAKEUP_PLAY_SET_DATE       "set-date"
#define WAKEUP_PLAY_SET_TIME       "set-time"
#define WAKEUP_PLAY_ENTER_ERROR  "pm-invalid-option"
#define WAKEUP_PLAY_MAX_NUMBER   "limit-maximum-number"

#define WAKEUP_PLAY_INPUT_YEAR    "please-enter-year"
#define WAKEUP_PLAY_INPUT_MONTH  "please-enter-month"
#define WAKEUP_PLAY_INPUT_DAY     "please-enter-day"
#define WAKEUP_PLAY_INPUT_HOUR    "please-enter-hour"
#define WAKEUP_PLAY_INPUT_MINUTE  "please-enter-minute"
#define WAKEUP_HA_SQL_SOCKET        "/data/HA/sql.socket"

typedef struct _wakeup_date{
	int year;
	int mouth;
	int day;
	int hour;
	int week;
	int minute;
	int second;
}WakeupDate;

#define ast_play_wakeup_voice_prompt(chan, content) ast_stream_and_wait(chan, content, AST_DIGIT_ANY)

#define ast_wakeup_play_and_wait_prompt(chan, content, result) \
	if (!result){ \
		result = ast_play_wakeup_voice_prompt(chan, content); \
	} \
	if (!result){ \
		result = ast_waitfordigit(chan, 800);\
	} \

#define apply_wakeup(flag_apply, exec_script) \
	if (flag_apply){ \
		cmd = system(exec_script); \
	} \

#define clean_wakeup_task(wakeup) \
	if (wakeup){ \
		free(wakeup); \
	} \

#define AST_UPDATE_WAKEUP_DATA(cmd, column, data_type, flag_date)   \
	snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "%s = '%s' ", column, data_type);\
	memset(data_type, 0, sizeof(data_type));\
	flag_date = 0;\

/**the status is modified to not handle wakeup**/
#define AST_WAKEUP_SET_DEF_STATUS(cmd)   \
	snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "UPDATE pms_wakeup SET w_action = '1' WHERE w_action = '2' ");\

#define AST_INPUT_TIMEOUT_QUIT(count, res) \
	if (!res) { \
		count++; \
		if (count > 2) { \
			res = 't'; \
			count = 0; \
		} \
	} \

#define AST_INPUT_THREE_ERROR_QUIT(count) \
	if (count == 3) { \
		return -1; \
	} \

#define wait_and_get_digital_keys(chan, buf, result, i, prompt_file) \
	memset(buf, '\0', sizeof(buf)); \
	result = ast_play_wakeup_voice_prompt(chan, prompt_file); \
	if (ast_readstring(chan, buf, sizeof(buf) - 1, 3000, 10000, "#") < 0) { \
		ast_log(AST_LOG_WARNING, "Unable to get digital keys \n"); \
		return -1; \
	} \
	if (result >= '0' && result <= '9') { \
		for(i = WAKEUP_MIN_LEN-1; i >= 1; i--){ \
			buf[i] = buf[i-1]; \
		} \
		buf[0] = (char)result; \
	} \

struct wakeup_info{
	int type;                                      //ucm wakeup:0   pms wakeup:1
	char extension[WAKEUP_MAX_LEN];           //wakeup extension
	char address[WAKEUP_MAX_LEN];            //room address
	char w_date[WAKEUP_MIN_LEN];             //year and mouth and day
	char w_time[WAKEUP_MIN_LEN];             //hour and minute
	char w_enable[WAKEUP_MIN_LEN];           // 0:cancel   1:set
	char w_type[WAKEUP_MIN_LEN];             // 1:single   2:dialy
	int send_status;                              //default:0
	int flag_address;                             //default:0
	int flag_date;                                //default:0
	int flag_time;                                //default:0
	int flag_enable;                              //default:0
	int flag_type;                                //default:0
	int exist_wakeup;                            //default:0
	int flag_set_wakeup;                         //default:0
	int flag_empty_date;                         //default:0
	int apply_wakeup;                           //default:0
	int repeats;                                 //default:0
	db_conn connection;
	int counts;
	int index;
};
int g_wakeup_ha_enable = 0;
static int get_number_limit_for_wakeup(void)
{
	char **dbresult = NULL;
	char *zErrMsg = NULL;
	int result = 0;
	char sql[512] = {0};
	char count[WAKEUP_MIN_LEN] = {0};
	int nRow = 0;
	int nCol = 0;
	db_conn tmp_conn = NULL;

	// Get device model
	FILE* fp = fopen( "/proc/gxvboard/dev_info/dev_alias", "r" );
	char* devModel = NULL;
	if ( fp != NULL ){
		size_t len = 0;
		if(getline(&devModel, &len, fp ) < 0){
			ast_log(LOG_ERROR,"get device error");
		}
		fclose( fp );
	}

	if (devModel) {
		int erropen;
		erropen = sqlite3_open(UCM_CONF_DB, &(tmp_conn));
		if(SQLITE_OK != erropen){
			ast_log(LOG_WARNING, "connect db failed!\n");
			sqlite3_close(tmp_conn);
			return -1;
		}

		snprintf(sql, sizeof(sql)-1, "select wakeup_service from feature_limits where id = '%s'; ", devModel);
		result = sqlite3_get_table(tmp_conn, sql, &dbresult, &nRow, &nCol, &zErrMsg);
		if (result != SQLITE_OK || *dbresult == NULL || *(dbresult + 1) == NULL || nRow == 0 || nCol == 0){
			sqlite3_free(zErrMsg);
			sqlite3_free_table(dbresult);
			sqlite3_close(tmp_conn);
			return -1;
		}
		strncpy(count, *(dbresult + 1), WAKEUP_MIN_LEN-1);
		wakeup_max_num = atoi(count);
		ast_debug(1, "the max number of wakeup is: %d", wakeup_max_num);

		sqlite3_free_table(dbresult);
		dbresult = NULL;
		sqlite3_close(tmp_conn);
		free(devModel);
	}

	return 0;
}

static int get_date_for_wakeup(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int count = 0;
	int i = WAKEUP_MIN_LEN-1;
	int res = 0;
	char buf[WAKEUP_MIN_LEN] = {0};
	WakeupDate tmp_date;
	int max_day[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

	memset(&tmp_date, 0, sizeof(WakeupDate));
	ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_DATE);
	/**get year when user input DTMF button**/
	for (count = 0; count < 3; count++) {
		wait_and_get_digital_keys(chan, buf, res, i, WAKEUP_PLAY_INPUT_YEAR);
		if (strlen(buf) == 4 && atoi(buf) >= 1) {
			sscanf(buf,"%4d", &(tmp_date.year));
			break;
		} else {
			ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_INVALID);
		}
	}
	AST_INPUT_THREE_ERROR_QUIT(count);
	 /**get month when user input DTMF button**/
	for (count = 0; count < 3; count++) {
		wait_and_get_digital_keys(chan, buf, res, i, WAKEUP_PLAY_INPUT_MONTH);
		if (atoi(buf) >= 1 && atoi(buf) <= 12 && strlen(buf) <= 2) {
			sscanf(buf,"%2d", &(tmp_date.mouth));
			break;
		} else {
			ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_INVALID);
		}
	}
	AST_INPUT_THREE_ERROR_QUIT(count);
	 /**get day when user input DTMF button**/
	for (count = 0; count < 3; count++) {
		wait_and_get_digital_keys(chan, buf, res, i, WAKEUP_PLAY_INPUT_DAY);
		if (atoi(buf) > 0 && atoi(buf) <= max_day[tmp_date.mouth] && strlen(buf) <= 2) {
			sscanf(buf,"%2d", &(tmp_date.day));
			break;
		} else if ((tmp_date.mouth == 2) && (atoi(buf) == 29) && (strlen(buf) <= 2) &&
		((tmp_date.year % 4 == 0 && tmp_date.year % 100 != 0) ||(tmp_date.year % 400 == 0))) {
			sscanf(buf,"%2d", &(tmp_date.day));
			break;
		} else {
			ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_INVALID);
		}
	}
	AST_INPUT_THREE_ERROR_QUIT(count);

	wakeup->flag_date = 1;
	if (wakeup->type == WAKEUP_TYPE_OF_UCM){
		snprintf(wakeup->w_date, WAKEUP_MIN_LEN-1, "%04d-%02d-%02d", tmp_date.year, tmp_date.mouth, tmp_date.day);

	}else{
		snprintf(wakeup->w_date, WAKEUP_MIN_LEN-1, "%04d%02d%02d", tmp_date.year, tmp_date.mouth, tmp_date.day);
	}

	return 0;
}

static int get_time_for_wakeup(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int count = 0;
	int i = WAKEUP_MIN_LEN-1;
	int res = 0;
	char buf[WAKEUP_MIN_LEN] = {0};
	WakeupDate tmp_time;

	memset(&tmp_time, 0, sizeof(WakeupDate));
	ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_TIME);
	/**get hour when user input DTMF button**/
	for (count = 0; count < 3; count++) {
		wait_and_get_digital_keys(chan, buf, res, i, WAKEUP_PLAY_INPUT_HOUR);
		if (atoi(buf) >= 0 && atoi(buf) < 24 && strlen(buf) <= 2) {
			sscanf(buf,"%2d", &(tmp_time.hour));
			break;
		} else {
			ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_INVALID);
		}
	}
	AST_INPUT_THREE_ERROR_QUIT(count);
	 /**get minute when user input DTMF button**/
	for (count = 0; count < 3; count++) {
		wait_and_get_digital_keys(chan, buf, res, i, WAKEUP_PLAY_INPUT_MINUTE);
		if (atoi(buf) >= 0 && atoi(buf) < 60 && strlen(buf) <= 2) {
			sscanf(buf,"%2d", &(tmp_time.minute));
			break;
		} else {
			ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_INVALID);
		}
	}
	AST_INPUT_THREE_ERROR_QUIT(count);

	wakeup->flag_time = 1;
	if (wakeup->type == WAKEUP_TYPE_OF_UCM){
		snprintf(wakeup->w_time, WAKEUP_MIN_LEN-1, "%02d:%02d", tmp_time.hour, tmp_time.minute);
	}else{
		snprintf(wakeup->w_time, WAKEUP_MIN_LEN-1, "%02d%02d", tmp_time.hour, tmp_time.minute);
	}

	return 0;

}

static struct wakeup_info *create_wakeup_task(void)
{
	struct wakeup_info *tmp_wakeup = NULL;
	tmp_wakeup = (struct wakeup_info *)malloc(WAKEUP_SIZE);
	if (tmp_wakeup){
		memset(tmp_wakeup, 0, WAKEUP_SIZE);
		snprintf(tmp_wakeup->w_type, sizeof(tmp_wakeup->w_type), "1");
		return tmp_wakeup;
	}else{
		return NULL;
	}
}

static int connect_ucm_db(struct wakeup_info *wakeup)
{
	int erropen;

	if (wakeup == NULL){
		return -1;
	}

	erropen = sqlite3_open(UCM_CONF_DB, &(wakeup->connection));
	if(SQLITE_OK != erropen){
		ast_log(LOG_WARNING, "connect db failed!\n");
		sqlite3_close(wakeup->connection);
		return -1;
	}

	return 0;
}

static void disconnect_ucm_db(struct wakeup_info *wakeup)
{
	sqlite3_close(wakeup->connection);
}

static int get_wakeup_name_from_db(struct wakeup_info *wakeup, char **wakeup_name)
{
	char **dbresult = NULL;
	char *zErrMsg = NULL;
	int result = 0;
	char sql[512] = {0};
	int nRow = 0;
	int nCol = 0;

	if (wakeup == NULL || wakeup_name == NULL) {
		return -1;
	}

	*wakeup_name = NULL;

	if (wakeup->exist_wakeup != 1) {
		return -1;
	}

	snprintf(sql, sizeof(sql)-1, "select wakeup_name from ucm_wakeup where extension = '%s'; ", wakeup->extension);
	result = sqlite3_get_table(wakeup->connection, sql, &dbresult, &nRow, &nCol, &zErrMsg);
	if (result != SQLITE_OK || *dbresult == NULL || *(dbresult + 1) == NULL || nRow != 1 || nCol == 0) {
		sqlite3_free(zErrMsg);
		sqlite3_free_table(dbresult);
		return -1;
	}

	*wakeup_name = ast_strdup(*(dbresult + 1));
	sqlite3_free_table(dbresult);
	return 0;
}

static void wakeup_send_config_reload_event(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	char content[WAKEUP_MAX_LEN + 32] = {0};
	char *wakeup_name = NULL;

	if (chan == NULL || wakeup == NULL) {
		return;
	}

	if (!wakeup->apply_wakeup) {
		return;
	}

	if (ast_strlen_zero(ast_channel_caller_num(chan))) {
		return;
	}

	if (wakeup->type == WAKEUP_TYPE_OF_UCM) {
		/*add*/
		if (wakeup->flag_set_wakeup) {
			snprintf(content, sizeof(content), "{\"wakeup_name\":\"%s\"}", wakeup->extension);
		} else {
			get_wakeup_name_from_db(wakeup, &wakeup_name);
			if (wakeup_name) {
				snprintf(content, sizeof(content), "{\"wakeup_name\":\"%s\"}", wakeup_name);
				ast_free(wakeup_name);
				wakeup_name = NULL;
			}
		}
	} else {
		snprintf(content, sizeof(content), "{\"address\":\"%s\"}", wakeup->address);
	}

	manager_event(EVENT_FLAG_CONFIG, "ConfigReload",
		"Date: %ld\r\n"
		"Source: extension\r\n"
		"UserName: %s\r\n"
		"Module: %s\r\n"
		"Action: %s\r\n"
		"Content: %s\r\n",
		time(NULL),
		ast_channel_caller_num(chan),
		(wakeup->type == WAKEUP_TYPE_OF_UCM ? "wakeup" : "pms_wakeup"),
		(wakeup->flag_set_wakeup ? "add" : "update"),
		content);
}

static int wakeup_get_haon(void)
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
    g_wakeup_ha_enable = atoi(on_buf);

    ast_log(LOG_DEBUG, "ha_enable is %d.", g_wakeup_ha_enable);
    return g_wakeup_ha_enable;

}

static void wakeup_send_ha_sql(const char *sql)
{
    int connect_fd = -1;
    struct sockaddr_un srv_addr;

    /*if ha is on, write it to file*/
    if (NULL == sql || 1 != g_wakeup_ha_enable)
    {
        return;
    }

    ast_log(LOG_DEBUG, "wakeup_send_ha_sql send buf is [%s] !", sql);

    /* remove the socket have the same name */
    connect_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if ( 0 >= connect_fd)
    {
        ast_log(LOG_ERROR, "wakeup_send_ha_sql creat socket fail: %d!", connect_fd);
        return;
    }
    /* set  server addr_param */
    srv_addr.sun_family = AF_UNIX;
    strncpy(srv_addr.sun_path, WAKEUP_HA_SQL_SOCKET, sizeof(srv_addr.sun_path) - 1);

    /* bind sockfd and addr */
    if(-1 == connect(connect_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr)))
    {
        close(connect_fd);
        return;
    }
    if (!send(connect_fd, sql, strlen(sql)+1, 0))
    {
        ast_log(LOG_ERROR, "wakeup_send_ha_sql send msg error, err_code is: %d!", connect_fd);
    }

    close(connect_fd);
}

static int update_wakeup_DB(const char *sql, db_conn connection)
{
	int err = 0;
	char *szErrMsg = 0;

	sqlite3_busy_timeout(connection, 5000);

	err = sqlite3_exec(connection,"BEGIN IMMEDIATE", NULL,NULL, &szErrMsg);
	if(SQLITE_OK != err){
		ast_log(LOG_WARNING, "begin error!\n");
		sqlite3_free(szErrMsg);
		return -1;
	}

	ast_log(LOG_DEBUG, "sql command: %s \n", sql);
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

    wakeup_send_ha_sql(sql);

	return 0;
}

static int get_address_from_db(const char *ext, struct wakeup_info *wakeup)
{
	char **dbresult = NULL;
	char *zErrMsg = NULL;
	int result = 0;
	char sql[512] = {0};
	char count[WAKEUP_MIN_LEN] = {0};
	int nRow = 0;
	int nCol = 0;

	if (ext == NULL || wakeup == NULL) {
		return -1;
	}
	snprintf(sql, sizeof(sql)-1, "select address from pms_room where extension = '%s'; ", ext);
	result = sqlite3_get_table(wakeup->connection, sql, &dbresult, &nRow, &nCol, &zErrMsg);
	if (result != SQLITE_OK || *dbresult == NULL || *(dbresult + 1) == NULL || nRow == 0 || nCol == 0){
		sqlite3_free(zErrMsg);
		sqlite3_free_table(dbresult);
		return -1;
	}
	strncpy(wakeup->address, *(dbresult + 1), WAKEUP_MAX_LEN);
	sqlite3_free_table(dbresult);
	dbresult = NULL;

	if (wakeup->address[0] != '\0') {
		wakeup->flag_address = 1;
		memset(sql, 0, sizeof(sql));
		ast_debug(1, "wakeup address is: %s \n", wakeup->address);
		snprintf(sql, sizeof(sql)-1, "select COUNT(*) from pms_wakeup where address = '%s'; ", wakeup->address);
		result = sqlite3_get_table(wakeup->connection, sql, &dbresult, NULL, NULL, &zErrMsg);
		if (result != SQLITE_OK || *dbresult == NULL || *(dbresult + 1) == NULL){
			sqlite3_free(zErrMsg);
			sqlite3_free_table(dbresult);
			return -1;
		}
		strncpy(count, *(dbresult + 1), WAKEUP_MIN_LEN-1);
		if (strcmp(count, "1") == 0) {
			wakeup->exist_wakeup = 1;
		}
		sqlite3_free_table(dbresult);
		dbresult = NULL;
	}else{
		wakeup->flag_address = 0;
	}

	return 0;
}

static int handle_empty_date_from_db(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	char **dbresult = NULL;
	char *zErrMsg = NULL;
	int result = 0;
	char sql[512] = {0};
	char count[WAKEUP_MIN_LEN] = {0};

	if (chan == NULL || wakeup == NULL) {
		return -1;
	}
	snprintf(sql, sizeof(sql)-1, "select COUNT(w_date) from pms_wakeup where address = '%s'; ", wakeup->address);
	result = sqlite3_get_table(wakeup->connection, sql, &dbresult, NULL, NULL, &zErrMsg);
	if (result != SQLITE_OK || *dbresult == NULL || *(dbresult + 1) == NULL){
		sqlite3_free(zErrMsg);
		sqlite3_free_table(dbresult);
		return -1;
	}
	strncpy(count, *(dbresult + 1), WAKEUP_MIN_LEN-1);
	if (strcmp(count, "0") == 0) {
		wakeup->flag_empty_date = 1;
	}else{
		wakeup->flag_empty_date = 0;
	}
	sqlite3_free_table(dbresult);
	dbresult = NULL;

	return 0;
}

static int get_wakeup_count(struct wakeup_info *wakeup)
{
	char **dbresult = NULL;
	char *zErrMsg = NULL;
	int result = 0;
	char sql[512] = {0};
	char count[WAKEUP_MIN_LEN] = {0};
	int nRow = 0;
	int nCol = 0;

	if (wakeup == NULL) {
		return -1;
	}
	snprintf(sql, sizeof(sql)-1, "select SUM(members_num) from ucm_wakeup; ");
	result = sqlite3_get_table(wakeup->connection, sql, &dbresult, &nRow, &nCol, &zErrMsg);
	if (result != SQLITE_OK || *dbresult == NULL || *(dbresult + 1) == NULL || nRow == 0 || nCol == 0){
		sqlite3_free(zErrMsg);
		sqlite3_free_table(dbresult);
		return -1;
	}
	strncpy(count, *(dbresult + 1), WAKEUP_MIN_LEN-1);
	wakeup->counts = atoi(count);

	sqlite3_free_table(dbresult);
	dbresult = NULL;

	return 0;
}

static int get_wakeup_index(struct wakeup_info *wakeup)
{
	char **dbresult = NULL;
	char *zErrMsg = NULL;
	int result = 0;
	char sql[512] = {0};
	char count[WAKEUP_MIN_LEN] = {0};
	int nRow = 0;
	int nCol = 0;

	if (wakeup == NULL) {
		return -1;
	}
	snprintf(sql, sizeof(sql)-1, "SELECT wakeup_index FROM ucm_wakeup LEFT JOIN "
		"(SELECT member_extension, COUNT(member_extension) AS num, wakeup_index AS tmp FROM ucm_wakeup_members GROUP BY wakeup_index) "
		"ON wakeup_index=tmp WHERE member_extension = '%s' AND num=1;", wakeup->extension);
	result = sqlite3_get_table(wakeup->connection, sql, &dbresult, &nRow, &nCol, &zErrMsg);
	if (result != SQLITE_OK || *dbresult == NULL || *(dbresult + 1) == NULL || nRow == 0 || nCol == 0){
		sqlite3_free(zErrMsg);
		sqlite3_free_table(dbresult);
		return -1;
	}
	strncpy(count, *(dbresult + 1), WAKEUP_MIN_LEN-1);
	wakeup->index = atoi(count);

	sqlite3_free_table(dbresult);
	dbresult = NULL;

	return 0;
}

static int exist_wakeup_extension(const char *ext, struct wakeup_info *wakeup)
{
	char **dbresult = NULL;
	char *zErrMsg = NULL;
	int result = 0;
	char sql[512] = {0};
	char count[WAKEUP_MIN_LEN] = {0};
	int nRow = 0;
	int nCol = 0;

	if (ext == NULL || wakeup == NULL) {
		return -1;
	}
	snprintf(sql, sizeof(sql)-1, "SELECT COUNT(*) FROM "
		"(SELECT * FROM ucm_wakeup LEFT JOIN (SELECT member_extension AS members, COUNT(member_extension) AS num, wakeup_index AS tmp FROM ucm_wakeup_members GROUP BY wakeup_index) "
		"ON wakeup_index=tmp WHERE members='%s' AND num=1); ", ext);
	result = sqlite3_get_table(wakeup->connection, sql, &dbresult, &nRow, &nCol, &zErrMsg);
	if (result != SQLITE_OK || *dbresult == NULL || *(dbresult + 1) == NULL || nRow == 0 || nCol == 0){
		sqlite3_free(zErrMsg);
		sqlite3_free_table(dbresult);
		return -1;
	}
	strncpy(count, *(dbresult + 1), WAKEUP_MIN_LEN-1);
	if (strcmp(count, "0") == 0) {
		wakeup->exist_wakeup = 0;
	} else {
		wakeup->exist_wakeup = atoi(count);
	}
	sqlite3_free_table(dbresult);
	dbresult = NULL;

	return wakeup->exist_wakeup;
}

static int wakeup_digital_instructions(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res = 0;
	if (chan == NULL || wakeup == NULL) {
		return -1;
	}else {
		/*set zero to clear cycle times*/
		wakeup->repeats = 0;
	}

	/* Play instructions and wait for new command */
	while (!res) {
		if (wakeup->exist_wakeup == 0){
			ast_wakeup_play_and_wait_prompt(chan, "press-one-to-add-wakeup-service", res);
		} else {
			if (wakeup->exist_wakeup == 1) {
				ast_wakeup_play_and_wait_prompt(chan, "press-two-to-modify-wakeup-service", res);
				ast_wakeup_play_and_wait_prompt(chan, "press-three-to-open-wakeup-service", res);
				ast_wakeup_play_and_wait_prompt(chan, "press-four-to-close-wakeup-service", res);
			}
			if (wakeup->type == WAKEUP_TYPE_OF_UCM) {
				ast_wakeup_play_and_wait_prompt(chan, "press-one-to-add-wakeup-service", res);
			}
		}
		if (!res) {
			res = ast_play_wakeup_voice_prompt(chan, "press-zero-to-exit");
		}

		if (!res) {
			res = ast_waitfordigit(chan, 6000);
		}
		AST_INPUT_TIMEOUT_QUIT( wakeup->repeats, res);
	}

	return res;
}

static int wakeup_init_intro(struct ast_channel *chan, struct wakeup_info *wk_info)
{
	int res = 0;
	if (chan == NULL || wk_info == NULL){
		return -1;
	}
	if (wk_info->exist_wakeup > 1) {
		ast_wakeup_play_and_wait_prompt(chan, "you-have-multiple-wake-up-services", res);
	}else if (wk_info->exist_wakeup == 1) {
		ast_wakeup_play_and_wait_prompt(chan, "you-have-wakeup-service", res);
	}else{
		ast_wakeup_play_and_wait_prompt(chan, "you-do-not-have-wakeup-service", res);
	}
	return res;
}

static int wakeup_instructions(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	return wakeup_digital_instructions(chan, wakeup);
}

static void clean_wakeup_data(struct wakeup_info *wakeup)
{
	memset(wakeup->w_date, 0, sizeof(wakeup->w_date));
	memset(wakeup->w_time, 0, sizeof(wakeup->w_time));
	memset(wakeup->w_enable, 0, sizeof(wakeup->w_enable));
	memset(wakeup->w_type, 0, sizeof(wakeup->w_type));
	wakeup->flag_date= 0;
	wakeup->flag_time= 0;
	wakeup->flag_enable= 0;
	wakeup->flag_type= 0;

	wakeup->send_status = 0;
	wakeup->flag_set_wakeup= 0;
	wakeup->apply_wakeup= 0;
	wakeup->repeats= 0;
}

static int add_type_for_wakeup(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res = 0;
	int count = 0;

	for (count = 0; count < 3; count++) {
		res = ast_play_wakeup_voice_prompt(chan, "please-press-one-to-set-single-wakeup-and-press-two-to-set-daily-wakeup");
		if (!res) {
			if (ast_readstring(chan, wakeup->w_type, sizeof(wakeup->w_type) - 1, 3000, 10000, "#") < 0) {
				ast_log(AST_LOG_WARNING, "Unable to add type \n");
				return -1;
			}
		}else{
			snprintf(wakeup->w_type, sizeof(wakeup->w_type) - 1, "%c", res);
		}
		if (strcmp(wakeup->w_type, WAKEUP_TYPE_OF_SINGLE) == 0 \
			&& (strchr(wakeup->w_type, '*') == NULL) && (wakeup->w_type[0] != '\0')) {
			wakeup->flag_type = 1;
			res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_EFFECTIVE);
			break;
		} else if (strcmp(wakeup->w_type, WAKEUP_TYPE_OF_DAILY) == 0 \
		&& (strchr(wakeup->w_type, '*') == NULL) && (wakeup->w_type[0] != '\0')) {
			wakeup->flag_type = 1;
			res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_EFFECTIVE);
			break;
		}else {
			count++;
			memset(wakeup->w_type, 0, sizeof(wakeup->w_type));
			res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_INVALID);
			AST_INPUT_THREE_ERROR_QUIT(count);
		}
	}

	return 0;
}

static int add_action_for_wakeup(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res = 0;
	int count = 0;

	for (count = 0; count < 3; count++) {
		res = ast_play_wakeup_voice_prompt(chan, "please-press-one-to-open-wakeup-service-and-press-zero-to-close-wakeup-service");
		if (!res) {
			if (ast_readstring(chan, wakeup->w_enable, sizeof(wakeup->w_enable) - 1, 3000, 10000, "#") < 0) {
				ast_log(AST_LOG_WARNING, "Unable to add action \n");
				return -1;
			}
		} else {
			snprintf(wakeup->w_enable, sizeof(wakeup->w_enable) - 1, "%c", res);
		}
		if (strcmp(wakeup->w_enable, WAKEUP_OPEN) == 0
			&& (strchr(wakeup->w_enable, '*') == NULL) && (wakeup->w_enable[0] != '\0')) {
			wakeup->flag_enable = 1;
			res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_EFFECTIVE);
			break;
		} else if (strcmp(wakeup->w_enable, WAKEUP_CLOSE) == 0
			&& (strchr(wakeup->w_enable, '*') == NULL) && (wakeup->w_enable[0] != '\0')) {
			wakeup->flag_enable = 1;
			res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_EFFECTIVE);
			break;
		}
		else {
			count++;
			memset(wakeup->w_enable, 0, sizeof(wakeup->w_enable));
			res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_INVALID);
			AST_INPUT_THREE_ERROR_QUIT(count);
		}
	}

	return 0;
}

static int add_wakeup_data(struct ast_channel *chan, struct wakeup_info *wakeup )
{
	int res =0;

	clean_wakeup_data(wakeup);
	ast_wakeup_play_and_wait_prompt(chan, "enter-wakeup-set", res);
	while ((res > -1) && (res != 'o') && (res != 't')) {
		//add date for wakeup ,eg:20160712
		if (!wakeup->flag_date){
			res = get_date_for_wakeup(chan, wakeup);
		}
		//add time for wakeup,eg:1600
		if (!res && !wakeup->flag_time){
			res = get_time_for_wakeup(chan, wakeup);
		}
		//add action for wakeup,eg:0(cancel),1(upset)
		if (!res && !wakeup->flag_enable){
			res = add_action_for_wakeup(chan, wakeup);
		}
		if (wakeup->type == WAKEUP_TYPE_OF_PMS){
			//add type for wakeup,eg:1(single), 2(daily)
			if (!res && !wakeup->flag_type){
				res = add_type_for_wakeup(chan, wakeup);
			}
			if (wakeup->flag_date && wakeup->flag_time && wakeup->flag_type && wakeup->flag_enable){
				wakeup->flag_set_wakeup = 1;
				res = 'o';
			}
		} else {
			if (wakeup->flag_date && wakeup->flag_time&& wakeup->flag_enable){
				wakeup->flag_set_wakeup = 1;
				res = 'o';
			}
		}
		AST_INPUT_TIMEOUT_QUIT(wakeup->repeats, res);
	}

	return res;
}

static int update_wakeup_instructions(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res = 0;
	while (!res) {
		if (wakeup->type == WAKEUP_TYPE_OF_PMS){
			/**fixed Bug 72268 by chzhdeng**/
			res = handle_empty_date_from_db(chan, wakeup);
			if (!wakeup->flag_type && (!wakeup->flag_empty_date)){
				ast_wakeup_play_and_wait_prompt(chan, "please-press-one-to-set-single-wakeup-and-press-two-to-set-daily-wakeup", res);
			}
		}
		if ((!wakeup->flag_date) && (!res)){
			ast_wakeup_play_and_wait_prompt(chan, "please-press-three-to-modify-date", res);
		}
		if ((!wakeup->flag_time) && (!res)){
			ast_wakeup_play_and_wait_prompt(chan, "please-press-four-to-modify-time", res);
		}
		if (!res) {
			ast_wakeup_play_and_wait_prompt(chan, "vm-starmain", res);
		}
		if (!res) {
			res = ast_play_wakeup_voice_prompt(chan, "press-zero-to-exit");
		}

		if (!res) {
			res = ast_waitfordigit(chan, 6000);
		}
		AST_INPUT_TIMEOUT_QUIT(wakeup->repeats, res);
	}

	return res;
}

static int update_ucm_wakeup_for_db(struct wakeup_info *wakeup)
{
	int res = -1;
	char sql_cmd[512] = {0};
	if (wakeup == NULL) {
		return res;
	}

	if (wakeup->index > 0) {
		snprintf(sql_cmd, sizeof(sql_cmd), "UPDATE ucm_wakeup set ");
		if (wakeup->flag_date) {
			AST_UPDATE_WAKEUP_DATA(sql_cmd, "custom_date",wakeup->w_date, wakeup->flag_date);
		}
		if (wakeup->flag_time){
			AST_UPDATE_WAKEUP_DATA(sql_cmd, "time",wakeup->w_time, wakeup->flag_time);
		}
		if (wakeup->flag_enable) {
			snprintf(sql_cmd + strlen(sql_cmd), sizeof(sql_cmd) - strlen(sql_cmd), "wakeup_enable = %d", atoi(wakeup->w_enable));
			memset(wakeup->w_enable, 0, sizeof(wakeup->w_enable));
			wakeup->flag_enable = 0;
		}
		snprintf(sql_cmd + strlen(sql_cmd), sizeof(sql_cmd) - strlen(sql_cmd), " WHERE wakeup_index = %d; ", wakeup->index);
	}
	res = update_wakeup_DB(sql_cmd, wakeup->connection);

	return res;
}

static int update_pms_wakeup_for_db(struct wakeup_info *wakeup)
{
	int res = -1;
	char sql_cmd[512] = {0};
	int len = 0;
	char exe_status[512] = {0};
	if (wakeup == NULL) {
		return res;
	}

	if (wakeup->flag_address) {
		snprintf(sql_cmd, sizeof(sql_cmd), "UPDATE pms_wakeup set ");
		if (wakeup->flag_date) {
			AST_UPDATE_WAKEUP_DATA(sql_cmd, "w_date",wakeup->w_date, wakeup->flag_date);
			AST_WAKEUP_SET_DEF_STATUS(exe_status);
		}
		if (wakeup->flag_time){
			AST_UPDATE_WAKEUP_DATA(sql_cmd, "w_time",wakeup->w_time, wakeup->flag_time);
			AST_WAKEUP_SET_DEF_STATUS(exe_status);
		}
		if (wakeup->flag_type) {
			AST_UPDATE_WAKEUP_DATA(sql_cmd, "w_type",wakeup->w_type, wakeup->flag_type);
			AST_WAKEUP_SET_DEF_STATUS(exe_status);
		}
		if (wakeup->flag_enable) {
			AST_UPDATE_WAKEUP_DATA(sql_cmd, "w_action",wakeup->w_enable, wakeup->flag_enable);
		}
		snprintf(sql_cmd + strlen(sql_cmd), sizeof(sql_cmd) - strlen(sql_cmd), ", send_status = 1 WHERE address = '%s'; ", wakeup->address);
	}

	if ((len = strlen(exe_status)) > 0) {
		snprintf(exe_status + strlen(exe_status), sizeof(exe_status) - strlen(exe_status), " AND address = '%s'; ", wakeup->address);
		res = update_wakeup_DB(exe_status, wakeup->connection);
	} else {
		res = 0;
	}
	if (!res) {
		res = update_wakeup_DB(sql_cmd, wakeup->connection);
	}
	manager_event(EVENT_FLAG_CALL, "PMSWakeupStatus",
		"Extension: %s\r\n"
		"WakeupActionStatus: 1\r\n"
		"WakeupAnswerStatus: 1\r\n"
		"WakeupDateStatus: %s\r\n"
		"WakeupType: %s\r\n"
		"WakeupTimeStatus: %s\r\n"
		"PMSActionStatus: update\r\n",
		wakeup->address?wakeup->address:"",
		wakeup->w_date?wakeup->w_date:"",
		wakeup->w_type?wakeup->w_type:"1",
		wakeup->w_time?wakeup->w_time:"");


	return res;
}

static int update_date_from_wakeup(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res = 0;

	//res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_DATE);
	if(get_date_for_wakeup(chan, wakeup)){
		return -1;
	}
	if (wakeup->type == WAKEUP_TYPE_OF_UCM){
			res = update_ucm_wakeup_for_db(wakeup);
	} else {
			res = update_pms_wakeup_for_db(wakeup);
	}
	if (!res) {
			res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_SUCCESS);
			wakeup->apply_wakeup = 1;
	} else {
			res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_FAILTURE);
	}

	return res;
}

static int update_time_from_wakeup(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res = 0;

	if (get_time_for_wakeup(chan, wakeup)){
		return -1;
	}
	if (wakeup->type == WAKEUP_TYPE_OF_UCM){
			res = update_ucm_wakeup_for_db(wakeup);
	} else {
			res = update_pms_wakeup_for_db(wakeup);
	}
	if (!res) {
			res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_SUCCESS);
			wakeup->apply_wakeup = 1;
	} else {
			res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_FAILTURE);
	}
	return res;
}

static int set_single_wakeup(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res = 0;

	strncpy(wakeup->w_type, WAKEUP_TYPE_OF_SINGLE, sizeof(wakeup->w_type-1));
	wakeup->flag_type = 1;
	res = update_pms_wakeup_for_db(wakeup);
	if (!res) {
		res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_SUCCESS);
		wakeup->apply_wakeup = 1;
	} else {
		res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_FAILTURE);
	}

	return res;
}

static int set_daily_wakeup(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res = 0;

	strncpy(wakeup->w_type, WAKEUP_TYPE_OF_DAILY, sizeof(wakeup->w_type)-1);
	wakeup->flag_type = 1;
	res = update_pms_wakeup_for_db(wakeup);
	if (!res) {
		res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_SUCCESS);
		wakeup->apply_wakeup = 1;
	} else {
		res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_FAILTURE);
	}

	return res;
}

static int enable_wakeup(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res = 0;

	wakeup->flag_enable = 1;
	strncpy(wakeup->w_enable, WAKEUP_OPEN, sizeof(wakeup->w_enable)-1);
	if(wakeup->type == WAKEUP_TYPE_OF_UCM){
		 res = update_ucm_wakeup_for_db(wakeup);
	} else {
		res = update_pms_wakeup_for_db(wakeup);
	}
	if (!res) {
		res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_SUCCESS);
		wakeup->apply_wakeup = 1;
	} else {
		 res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_FAILTURE);
	}

	return res;
}

static int disable_wakeup(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res = 0;

	wakeup->flag_enable = 1;
	strncpy(wakeup->w_enable, WAKEUP_CLOSE, sizeof(wakeup->w_enable)-1);
	if(wakeup->type == WAKEUP_TYPE_OF_UCM){
		res = update_ucm_wakeup_for_db(wakeup);
	} else {
		res = update_pms_wakeup_for_db(wakeup);
	}
	if (!res) {
		res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_SUCCESS);
		wakeup->apply_wakeup = 1;
	} else {
		 res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_FAILTURE);
	}

	return res;
}

static int add_wakeup_information_from_sql(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res = -1;
	char sql_cmd[512] = {0};
	char tmp_name[256] = {0};
	time_t nowtime;
	struct tm * timeinfo = NULL;
	if (wakeup == NULL) {
		return res;
	}

	if (wakeup->flag_set_wakeup) {
		if (wakeup->type == WAKEUP_TYPE_OF_UCM){
			snprintf(tmp_name, sizeof(tmp_name)-1, "%s_", wakeup->extension);
			time (&nowtime);
			timeinfo = localtime (&nowtime);
			strftime (tmp_name + strlen(tmp_name), sizeof(tmp_name) - strlen(tmp_name), "%Y%m%d%H%M%S", timeinfo);
			snprintf(sql_cmd, sizeof(sql_cmd), "INSERT INTO ucm_wakeup (wakeup_name, wakeup_enable, extension, custom_date, time, members_num) VALUES ('%s', %d, '%s', '%s', '%s', 1);",
				tmp_name, atoi(wakeup->w_enable), wakeup->extension, wakeup->w_date, wakeup->w_time);
			res = update_wakeup_DB(sql_cmd, wakeup->connection);
			if (res == 0){
				memset(sql_cmd, '\0', sizeof(sql_cmd));
				snprintf(sql_cmd, sizeof(sql_cmd), "INSERT INTO ucm_wakeup_members (wakeup_index, member_extension) SELECT wakeup_index, '%s' AS member_extension FROM ucm_wakeup WHERE wakeup_name = '%s'; ",
					wakeup->extension, tmp_name);
				res = update_wakeup_DB(sql_cmd, wakeup->connection);
			}
		}else{
			snprintf(sql_cmd, sizeof(sql_cmd), "INSERT INTO pms_wakeup (address, w_date, w_time, w_type, w_action, send_status) VALUES ('%s', '%s', '%s', '%s', '%s', 1);",
				wakeup->address, wakeup->w_date, wakeup->w_time, wakeup->w_type, wakeup->w_enable);
			res = update_wakeup_DB(sql_cmd, wakeup->connection);
			manager_event(EVENT_FLAG_CALL, "PMSWakeupStatus",
				"Extension: %s\r\n"
				"WakeupActionStatus: %s\r\n"
				"WakeupAnswerStatus: 1\r\n"
				"WakeupDateStatus: %s\r\n"
				"WakeupType: %s\r\n"
				"WakeupTimeStatus: %s\r\n"
				"PMSActionStatus: add\r\n",
				wakeup->address?wakeup->address:"",
				wakeup->w_enable?wakeup->w_enable:"0",
				wakeup->w_date?wakeup->w_date:"",
				wakeup->w_type?wakeup->w_type:"1",
				wakeup->w_time?wakeup->w_time:"");
		}
	}
	if (res == 0){
		wakeup->apply_wakeup = 1;
		wakeup->exist_wakeup = 1;
		res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_SUCCESS);
	}else{
		res = ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_SET_FAILTURE);
	}
	res = 'q';

	return res;
}

static int add_wakeup_instructions(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res = 0;
	int repeats = 0;

	while (!res) {
		if (wakeup->flag_set_wakeup) {
			ast_wakeup_play_and_wait_prompt(chan, "press-one-to-confirm-add-wakeup-service", res);
		}
		if (!res) {
			res = ast_play_wakeup_voice_prompt(chan, "press-zero-to-exit");
		}

		if (!res) {
			res = ast_waitfordigit(chan, 6000);
		}
		AST_INPUT_TIMEOUT_QUIT(repeats, res)

	}
	return res;
}

static int add_wakeup_service(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res = 0;
	if (chan == NULL || wakeup == NULL) {
		return -1;
	}

	res = add_wakeup_data(chan, wakeup);
	while ((res > -1) && (res != 'q') && (res != 't') && (res != '#')) {
		switch (res) {
			case '0': /*quit*/
				res = 'q';
				break;
			case '1': /*update wakeup*/
				res = add_wakeup_information_from_sql(chan, wakeup);
				break;
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':/*set invalid*/
				ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_ENTER_ERROR);
				res = 0;
				break;
			case '*':
			case '#':
				res = 0;
				break;
			default:    /* Nothing and prompt instructions*/
				res = add_wakeup_instructions(chan, wakeup);
				break;
		}
	}

	return res;
}
static int update_wakeup_service(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int res =0;
	if (chan == NULL || wakeup == NULL) {
		return -1;
	}
	while ((res > -1) && (res != 'q') && (res != 't') && (res != '*')) {
		switch (res) {
			case '0': //quit
				res = 'q';
				break;
			case '1'://update type:singe(1), daily(2)
				res = handle_empty_date_from_db(chan, wakeup);
				if (wakeup->type == WAKEUP_TYPE_OF_PMS && (!wakeup->flag_empty_date)){
					res = set_single_wakeup(chan, wakeup);
				}else{
					/*Don't need to do anything.*/
					res = '9';
				}
				break;
			case '2'://update action:cancel(0),set(1)
				if (wakeup->type == WAKEUP_TYPE_OF_PMS && (!wakeup->flag_empty_date)){
					res = set_daily_wakeup(chan, wakeup);
				}else{
					/*Don't need to do anything.*/
					res = '9';
				}
				break;
			case '3': //update date
				res = update_date_from_wakeup(chan, wakeup);
				break;
			case '4': //update time
				res = update_time_from_wakeup(chan, wakeup);
				break;
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':/*set invalid*/
				ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_ENTER_ERROR);
				res = 0;
				break;
			case '#':
				res = 0;
				break;
			default:    //Nothing, prompt tone
				res = update_wakeup_instructions(chan, wakeup);
				break;
		}
	}

	return res;
}

static int execute_wakeup(struct ast_channel *chan, struct wakeup_info *wakeup)
{
	int cmd;

	cmd = wakeup_init_intro(chan, wakeup);
	while ((cmd > -1) && (cmd != 'q') && (cmd != 't')) {
		/* Run main menu */
		switch (cmd) {
			case '0':/*quit wakeup*/
				cmd = 'q';
				break;
			case '1': /*insert wakeup*/
				if ((!wakeup->exist_wakeup) || (wakeup->type == WAKEUP_TYPE_OF_UCM)){
					/***calculate the number of ucm wakeup,if it exceed the maximum, please quit***/
					if (wakeup->type == WAKEUP_TYPE_OF_UCM){
						get_wakeup_count(wakeup);
						ast_debug(1, "added wakeup number: %d, max wakeup number: %d", wakeup->counts, wakeup_max_num);
						if (wakeup->counts >= wakeup_max_num) {
							ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_MAX_NUMBER);
							cmd = 'q';
						}
					}

					if (cmd != 'q'){
						cmd = add_wakeup_service(chan, wakeup);
					}
				}else{
					/*Don't need to do anything.*/
					ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_ENTER_ERROR);
					cmd = 0;
				}
				break;
			case '2': /*update wakeup*/
				if (wakeup->exist_wakeup == 1){
					cmd = update_wakeup_service(chan, wakeup);
				}else{
					/*Don't need to do anything.*/
					ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_ENTER_ERROR);
					cmd = 0;
				}
				break;
			case '3': /*update wakeup*/
				if (wakeup->exist_wakeup == 1){
					cmd = enable_wakeup(chan, wakeup);
				}else{
					/*Don't need to do anything.*/
					ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_ENTER_ERROR);
					cmd = 0;
				}
				break;
			case '4': /*update wakeup*/
				if (wakeup->exist_wakeup == 1){
					cmd = disable_wakeup(chan, wakeup);
				}else{
					/*Don't need to do anything.*/
					ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_ENTER_ERROR);
					cmd = 0;
				}
				break;
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':/*set invalid*/
				ast_play_wakeup_voice_prompt(chan, WAKEUP_PLAY_ENTER_ERROR);
				cmd = 0;
				break;
			case '*':
			case '#':
				cmd = 0;
				break;
			default:    /* Nothing and prompt instructions*/
				cmd = wakeup_instructions(chan, wakeup);
				break;
		}
	}
	return cmd;
}

static int pms_wakeup_exec(struct ast_channel *chan, const char *data)
{
	int cmd = 0;
	struct wakeup_info *wakeup = NULL;

	ast_debug(1, "enter the apply of PMS wakeup! \n");
	wakeup = create_wakeup_task();
	if (wakeup == NULL){
		return -1;
	}
	wakeup->type = WAKEUP_TYPE_OF_PMS;

	if (connect_ucm_db(wakeup)){
		clean_wakeup_task(wakeup);
		return -1;
	}

	if (!ast_strlen_zero(ast_channel_caller_num(chan))) {
		ast_debug(1, "extension number is: %s \n", ast_channel_caller_num(chan));
		cmd = get_address_from_db(ast_channel_caller_num(chan), wakeup);
		if (!wakeup->flag_address){
			ast_play_wakeup_voice_prompt(chan, "the-extension-is-not-used-for-hotel");
			disconnect_ucm_db(wakeup);
			clean_wakeup_task(wakeup);
			return -1;
		}
	}else {
		ast_debug(1, "extension number don't exist !\n");
		disconnect_ucm_db(wakeup);
		clean_wakeup_task(wakeup);
		return -1;
	}

	cmd = execute_wakeup(chan, wakeup);
	if (cmd == 0 || cmd == 'q' || cmd == -1){
		apply_wakeup(wakeup->apply_wakeup, AST_PMS_WAKEUP_CRONTAB);
		wakeup_send_config_reload_event(chan, wakeup);
	}
	disconnect_ucm_db(wakeup);
	clean_wakeup_task(wakeup);
	ast_debug(1, "end the apply of WakeUp! \n");
	return 0;
}

static int ucm_wakeup_exec(struct ast_channel *chan, const char *data)
{
	int cmd;
	struct wakeup_info *wakeup = NULL;

	ast_debug(1, "enter the apply of ucm wakeup! \n");
	wakeup = create_wakeup_task();
	if (wakeup == NULL){
		return -1;
	}
	wakeup->type = WAKEUP_TYPE_OF_UCM;
	wakeup->index = 0;

	if (connect_ucm_db(wakeup)){
		clean_wakeup_task(wakeup);
		return -1;
	}

	if (!ast_strlen_zero(ast_channel_caller_num(chan))) {
		ast_debug(1, "extension number is: %s \n", ast_channel_caller_num(chan));
		strncpy(wakeup->extension, (char *)ast_channel_caller_num(chan), sizeof(wakeup->extension));
		exist_wakeup_extension(ast_channel_caller_num(chan), wakeup);
		if (wakeup->exist_wakeup == 1){
			get_wakeup_index(wakeup);
		}
	}else {
		ast_debug(1, "extension number don't exist !\n");
		disconnect_ucm_db(wakeup);
		clean_wakeup_task(wakeup);
		return -1;
	}

	cmd = execute_wakeup(chan, wakeup);
	if (cmd == 0 || cmd == 'q' || cmd == -1){
		apply_wakeup(wakeup->apply_wakeup, AST_UCM_WAKEUP_CRONTAB);
		wakeup_send_config_reload_event(chan, wakeup);
	}
	disconnect_ucm_db(wakeup);
	clean_wakeup_task(wakeup);
	ast_debug(1, "end the apply of UCM WakeUp! \n");
	return 0;

}

static int unload_module(void)
{
	int res;
	res = ast_unregister_application(app);
	res |= ast_unregister_application(app2);
	return res;
}

static int load_module(void)
{
	int res;
    wakeup_get_haon();
	res = ast_register_application_xml(app, pms_wakeup_exec);
	res |= ast_register_application_xml(app2, ucm_wakeup_exec);

	get_number_limit_for_wakeup();

	return res;
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Trivial WakeUp Application");

#endif
