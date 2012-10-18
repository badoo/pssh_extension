/*
  +----------------------------------------------------------------------+
  | PHP Version 4                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2006 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Antony Dovgal <tony@daylessday.org>                          |
  +----------------------------------------------------------------------+
*/

/* $Id: pssh.c,v 1.8 2008/05/06 12:22:04 tony Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_pssh.h"

#include <pssh.h>

static int le_pssh_session;
static int le_pssh_tasklist;
static int le_pssh_task;

/* {{{ internal types */
typedef struct _php_pssh_session {
	pssh_session_t *s;
	int rsrc_id;
} php_pssh_session;

typedef struct _php_pssh_tasklist {
	pssh_task_list_t *tl;
	int rsrc_id;
	php_pssh_session *s;
} php_pssh_tasklist;

typedef struct _php_pssh_task {
	struct pssh_task_t *t;
	int rsrc_id;
	php_pssh_tasklist *tl;
} php_pssh_task;
/* }}} */

#ifdef COMPILE_DL_PSSH
ZEND_GET_MODULE(pssh)
#endif

#define PHP_PSSH_HOSTADDR_LEN 100

#define PHP_ZVAL_TO_SESSION(zval, s) \
		ZEND_FETCH_RESOURCE(s, php_pssh_session *, &zval, -1, "pssh session", le_pssh_session)

#define PHP_ZVAL_TO_TASKLIST(zval, tl) \
		ZEND_FETCH_RESOURCE(tl, php_pssh_tasklist *, &zval, -1, "pssh tasklist", le_pssh_tasklist)

#define PHP_ZVAL_TO_TASK(zval, t) \
		ZEND_FETCH_RESOURCE(t, php_pssh_task *, &zval, -1, "pssh task", le_pssh_task)

static void _pssh_session_list_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) /* {{{ */
{
	php_pssh_session *s = (php_pssh_session *)rsrc->ptr;

	pssh_free(s->s);
	efree(s);
}
/* }}} */

static void _pssh_tasklist_list_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) /* {{{ */
{
	php_pssh_tasklist *tl = (php_pssh_tasklist *)rsrc->ptr;

	pssh_task_list_free(tl->tl);
	zend_list_delete(tl->s->rsrc_id);
	efree(tl);
}
/* }}} */

static void _pssh_task_list_dtor(zend_rsrc_list_entry *rsrc TSRMLS_DC) /* {{{ */
{
	php_pssh_task *t = (php_pssh_task *)rsrc->ptr;

	zend_list_delete(t->tl->rsrc_id);
	efree(t);
}
/* }}} */

/* {{{ proto resource pssh_init(string username, string pubkey[, string privkey[, string password[, int options]]]) */
static PHP_FUNCTION(pssh_init)
{
	php_pssh_session *s;
	char *username, *pubkey, *privkey = NULL, *password = NULL;
	int username_len, pubkey_len, privkey_len, password_len;
	long options = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss|ssl", &username, &username_len, &pubkey, &pubkey_len, &privkey, &privkey_len, &password, &password_len, &options) != SUCCESS) {
		return;
	}

	if (privkey != NULL && privkey_len == 0) {
		privkey = NULL;
	}
	
	if (password != NULL && password_len == 0) {
		password = NULL;
	}

	s = emalloc(sizeof(php_pssh_session));
	s->s = pssh_init(username, pubkey, privkey, password, (int)options);

	if (!s->s) {
		/* ssh-agent is not running or public key was not added properly */
		efree(s);
		RETURN_FALSE;
	}

	s->rsrc_id = zend_list_insert(s, le_pssh_session);
	RETURN_RESOURCE(s->rsrc_id);
}
/* }}} */

/* {{{ proto void pssh_free(resource session) */
static PHP_FUNCTION(pssh_free)
{
	zval *zs;
	php_pssh_session *s;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zs) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_SESSION(zs, s);

	zend_list_delete(s->rsrc_id);
}
/* }}} */

/* {{{ proto bool pssh_server_add(resource session, string server[, int port]) */
static PHP_FUNCTION(pssh_server_add)
{
	zval *zs;
	php_pssh_session *s;
	char *server;
	int server_len;
	long port = 22;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs|l", &zs, &server, &server_len, &port) != SUCCESS) {
		return;
	}

	if (server_len > PHP_PSSH_HOSTADDR_LEN) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "server name exceeds the maximum allowed length of %d characters", PHP_PSSH_HOSTADDR_LEN);
		RETURN_FALSE;
	}

	PHP_ZVAL_TO_SESSION(zs, s);

	if (pssh_server_add(s->s, (const char *)server, (int)port) != 0) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto array pssh_server_first(resource session) */
static PHP_FUNCTION(pssh_server_first)
{
	zval *zs;
	php_pssh_session *s;
	struct pssh_sess_entry *entry;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zs) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_SESSION(zs, s);

	entry = pssh_server_first(s->s);
	if (entry == NULL) {
		RETURN_FALSE;
	}

	array_init(return_value);
	add_assoc_string_ex(return_value, "name", sizeof("name"), pssh_serv_name(entry), 1);
	add_assoc_long_ex(return_value, "port", sizeof("port"), pssh_serv_port(entry));
}
/* }}} */

/* {{{ proto array pssh_server_next(resource session) */
static PHP_FUNCTION(pssh_server_next)
{
	zval *zs;
	php_pssh_session *s;
	struct pssh_sess_entry *entry;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zs) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_SESSION(zs, s);

	entry = pssh_server_next(s->s);
	if (entry == NULL) {
		RETURN_FALSE;
	}

	array_init(return_value);
	add_assoc_string_ex(return_value, "name", sizeof("name"), pssh_serv_name(entry), 1);
	add_assoc_long_ex(return_value, "port", sizeof("port"), pssh_serv_port(entry));
}
/* }}} */

/* {{{ proto int pssh_server_status(resource session, string server) */
static PHP_FUNCTION(pssh_server_status)
{
	zval *zs;
	php_pssh_session *s;
	char *server, *entry_name;
	int server_len, entry_name_len;
	struct pssh_sess_entry *entry;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rs", &zs, &server, &server_len) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_SESSION(zs, s);

	for (entry = pssh_server_first(s->s); entry != NULL; entry = pssh_server_next(s->s)) {
		entry_name = pssh_serv_name(entry);
		entry_name_len = 0;
		if (entry_name) {
			entry_name_len = strlen(entry_name);
			if (entry_name_len == server_len && memcmp(server, entry_name, server_len) == 0){
				/* found */
				break;
			}
		}
	}

	if (entry == NULL) {
		RETURN_FALSE;
	}
	RETURN_LONG(pssh_stat(entry));
}
/* }}} */

/* {{{ proto int pssh_connect(resource session, int timeout) */
static PHP_FUNCTION(pssh_connect)
{
	zval *zs, *server_name;
	struct pssh_sess_entry *entry = NULL;
	php_pssh_session *s;
	long timeout;
	int ret;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rzl", &zs, &server_name, &timeout) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_SESSION(zs, s);

	ret = pssh_connect(s->s, &entry, (int)timeout);
	zval_dtor(server_name);
	if (entry) {
		ZVAL_STRING(server_name, pssh_serv_name(entry), 1);
	} else {
		ZVAL_NULL(server_name);
	}

	RETURN_LONG(ret);
}
/* }}} */

/* {{{ proto array pssh_servers_list(resource session) */
static PHP_FUNCTION(pssh_servers_list)
{
	zval *zs;
	php_pssh_session *s;
	struct pssh_sess_entry *entry;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zs) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_SESSION(zs, s);

	array_init(return_value);

	for (entry = pssh_server_first(s->s); entry != NULL; entry = pssh_server_next(s->s)) {
		zval *tmp;
		
		ALLOC_INIT_ZVAL(tmp);
		array_init(tmp);

		add_assoc_string_ex(tmp, "name", sizeof("name"), pssh_serv_name(entry), 1);
		add_assoc_long_ex(tmp, "port", sizeof("port"), pssh_serv_port(entry));

		add_next_index_zval(return_value, tmp);
	}
}
/* }}} */

/* {{{ proto resource pssh_tasklist_init(resource session) */
static PHP_FUNCTION(pssh_tasklist_init)
{
	zval *zs;
	php_pssh_session *s;
	php_pssh_tasklist *tl;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &zs) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_SESSION(zs, s);

	tl = emalloc(sizeof(php_pssh_tasklist));

	zend_list_addref(s->rsrc_id);
	tl->tl = pssh_task_list_init(s->s);
	tl->s = s;

	tl->rsrc_id = zend_list_insert(tl, le_pssh_tasklist);
	RETURN_RESOURCE(tl->rsrc_id);
}
/* }}} */

/* {{{ proto void pssh_tasklist_free(resource tasklist) */
static PHP_FUNCTION(pssh_tasklist_free)
{
	zval *ztl;
	php_pssh_tasklist *tl;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ztl) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASKLIST(ztl, tl);

	zend_list_delete(tl->rsrc_id);
}
/* }}} */

/* {{{ proto bool pssh_copy_to_server(resource tasklist, string server, string local_file, string remote_file) */
static PHP_FUNCTION(pssh_copy_to_server)
{
	zval *ztl;
	php_pssh_tasklist *tl;
	int ret;
	char *server, *local_file, *remote_file;
	int server_len, local_file_len, remote_file_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rsss", &ztl, 
																 &server, &server_len, 
																 &local_file, &local_file_len,
																 &remote_file, &remote_file_len) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASKLIST(ztl, tl);

	ret = pssh_cp_to_server(tl->tl, server, local_file, remote_file);
	if (ret != 0) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool pssh_copy_from_server(resource tasklist, string server, string remote_file, string local_file) */
static PHP_FUNCTION(pssh_copy_from_server)
{
	zval *ztl;
	php_pssh_tasklist *tl;
	int ret;
	char *server, *local_file, *remote_file;
	int server_len, local_file_len, remote_file_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rsss", &ztl, 
																 &server, &server_len, 
																 &remote_file, &remote_file_len,
																 &local_file, &local_file_len) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASKLIST(ztl, tl);

	ret = pssh_cp_from_server(tl->tl, server, local_file, remote_file);
	if (ret != 0) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool pssh_tasklist_add(resource tasklist, string server, string cmd) */
static PHP_FUNCTION(pssh_tasklist_add)
{
	zval *ztl;
	php_pssh_tasklist *tl;
	int ret;
	char *server, *cmd;
	int server_len, cmd_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rss", &ztl, 
																 &server, &server_len, 
																 &cmd, &cmd_len) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASKLIST(ztl, tl);

	ret = pssh_add_cmd(tl->tl, server, cmd);
	if (ret != 0) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int pssh_tasklist_exec(resource tasklist, int timeout) */
static PHP_FUNCTION(pssh_tasklist_exec)
{
	zval *ztl, *server_name;
	php_pssh_tasklist *tl;
	struct pssh_task_t *task = NULL;
	int ret;
	long timeout;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "rzl", &ztl, &server_name, &timeout) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASKLIST(ztl, tl);

	ret = pssh_exec(tl->tl, &task, (int)timeout);
	zval_dtor(server_name);
	if (task) {
		ZVAL_STRING(server_name, pssh_task_server_name(task), 1);
	} else {
		ZVAL_NULL(server_name);
	}
	
	RETURN_LONG(ret);
}
/* }}} */

/* {{{ proto resource pssh_tasklist_first(resource tasklist) */
static PHP_FUNCTION(pssh_tasklist_first)
{
	zval *ztl;
	php_pssh_tasklist *tl;
	php_pssh_task *t;
	struct pssh_task_t *task;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ztl) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASKLIST(ztl, tl);

	task = pssh_task_first(tl->tl);
	if (task == NULL) {
		RETURN_FALSE;
	}

	zend_list_addref(tl->rsrc_id);

	t = emalloc(sizeof(php_pssh_task));
	t->t = task;
	t->tl = tl;

	t->rsrc_id = zend_list_insert(t, le_pssh_task);
	RETURN_RESOURCE(t->rsrc_id);
}
/* }}} */

/* {{{ proto resource pssh_tasklist_next(resource tasklist) */
static PHP_FUNCTION(pssh_tasklist_next)
{
	zval *ztl;
	php_pssh_tasklist *tl;
	php_pssh_task *t;
	struct pssh_task_t *task;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &ztl) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASKLIST(ztl, tl);

	task = pssh_task_next(tl->tl);
	if (task == NULL) {
		RETURN_FALSE;
	}

	zend_list_addref(tl->rsrc_id);

	t = emalloc(sizeof(php_pssh_task));
	t->t = task;
	t->tl = tl;

	t->rsrc_id = zend_list_insert(t, le_pssh_task);
	RETURN_RESOURCE(t->rsrc_id);
}
/* }}} */

/* {{{ proto string pssh_task_server_name(resource task) */
static PHP_FUNCTION(pssh_task_server_name)
{
	zval *zt;
	php_pssh_task *t;
	char *server;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zt) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASK(zt, t);

	server = pssh_task_server_name(t->t);
	RETURN_STRING(server, 1);
}
/* }}} */

/* {{{ proto int pssh_task_status(resource task) */
static PHP_FUNCTION(pssh_task_status)
{
	zval *zt;
	php_pssh_task *t;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zt) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASK(zt, t);

	RETURN_LONG(pssh_task_stat(t->t));
}
/* }}} */

/* {{{ proto string pssh_task_stderr(resource task) */
static PHP_FUNCTION(pssh_task_stderr)
{
	zval *zt;
	php_pssh_task *t;
	char *err;
	int err_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zt) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASK(zt, t);

	err = pssh_task_stderr(t->t);
	err_len = pssh_task_stderr_len(t->t);

	if (err != NULL && err_len > 0) {
		RETURN_STRINGL(err, err_len, 1);
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto string pssh_task_stdout(resource task) */
static PHP_FUNCTION(pssh_task_stdout)
{
	zval *zt;
	php_pssh_task *t;
	char *out;
	int out_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zt) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASK(zt, t);

	out = pssh_task_stdout(t->t);
	out_len = pssh_task_stdout_len(t->t);

	if (out != NULL && out_len > 0) {
		RETURN_STRINGL(out, out_len, 1);
	}
	RETURN_FALSE;
}
/* }}} */

/* {{{ proto int pssh_task_exit_status(resource task) */
static PHP_FUNCTION(pssh_task_exit_status)
{
	zval *zt;
	php_pssh_task *t;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zt) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASK(zt, t);

	RETURN_LONG(pssh_task_exit_status(t->t));
}
/* }}} */

/* {{{ proto int pssh_task_type(resource task) */
static PHP_FUNCTION(pssh_task_type)
{
	zval *zt;
	php_pssh_task *t;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zt) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASK(zt, t);

	RETURN_LONG(pssh_task_type(t->t));
}
/* }}} */

/* {{{ proto int pssh_task_get_cmd(resource task) */
static PHP_FUNCTION(pssh_task_get_cmd)
{
	zval *zt;
	php_pssh_task *t;
	char *cmd;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "r", &zt) != SUCCESS) {
		return;
	}

	PHP_ZVAL_TO_TASK(zt, t);

	cmd = pssh_task_get_cmd(t->t);
	if (!cmd) {
		RETURN_FALSE;
	}
	RETURN_STRING(cmd, 1);
}
/* }}} */

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(pssh)
{
	le_pssh_session = zend_register_list_destructors_ex(_pssh_session_list_dtor, NULL, "pssh session", module_number);
	le_pssh_tasklist = zend_register_list_destructors_ex(_pssh_tasklist_list_dtor, NULL, "pssh tasklist", module_number);
	le_pssh_task = zend_register_list_destructors_ex(_pssh_task_list_dtor, NULL, "pssh task", module_number);

	REGISTER_LONG_CONSTANT("PSSH_CONNECTED", PSSH_CONNECTED, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PSSH_RUNNING", PSSH_RUNNING, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PSSH_TIMEOUT", PSSH_TIMEOUT, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PSSH_FAILED", PSSH_FAILED, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PSSH_SUCCESS", PSSH_SUCCESS, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("PSSH_ERROR", PSSH_ERROR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PSSH_INPROGRESS", PSSH_INPROGRESS, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PSSH_DONE", PSSH_DONE, CONST_CS | CONST_PERSISTENT);
//	REGISTER_LONG_CONSTANT("PSSH_NONE", PSSH_NONE, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("PSSH_TASK_TYPE_COPY", PSSH_TASK_TYPE_COPY, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PSSH_TASK_TYPE_EXEC", PSSH_TASK_TYPE_EXEC, CONST_CS | CONST_PERSISTENT);

//	REGISTER_LONG_CONSTANT("PSSH_TASK_NONE", PSSH_TASK_NONE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PSSH_TASK_ERROR", PSSH_TASK_ERROR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PSSH_TASK_INPROGRESS", PSSH_TASK_INPROGRESS, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("PSSH_TASK_DONE", PSSH_TASK_DONE, CONST_CS | CONST_PERSISTENT);
	
	REGISTER_LONG_CONSTANT("PSSH_OPT_NO_SEARCH", PSSH_OPT_NO_SEARCH, CONST_CS | CONST_PERSISTENT);

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(pssh)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "pssh support", "enabled");
	php_info_print_table_row(2, "Version", PHP_PSSH_VERSION);
	php_info_print_table_end();
}
/* }}} */

#if defined(PHP_VERSION_ID) && PHP_VERSION_ID > 50000
ZEND_BEGIN_ARG_INFO(a3_arg_force_ref, 3)
	 ZEND_ARG_PASS_INFO(0)
	 ZEND_ARG_PASS_INFO(1)
	 ZEND_ARG_PASS_INFO(0)
ZEND_END_ARG_INFO();
#else
static unsigned char a3_arg_force_ref[] = { 3, BYREF_NONE, BYREF_FORCE, BYREF_NONE };
#endif

/* {{{ pssh_functions[]
 */
zend_function_entry pssh_functions[] = {
	PHP_FE(pssh_init,	NULL)
	PHP_FE(pssh_free,	NULL)
	PHP_FE(pssh_server_add,	NULL)
	PHP_FE(pssh_server_first,	NULL)
	PHP_FE(pssh_server_next,	NULL)
	PHP_FE(pssh_server_status,	NULL)
	PHP_FE(pssh_connect,	a3_arg_force_ref)
	PHP_FE(pssh_servers_list,	NULL)
	PHP_FE(pssh_tasklist_init,	NULL)
	PHP_FE(pssh_tasklist_free,	NULL)
	PHP_FE(pssh_copy_to_server,	NULL)
	PHP_FE(pssh_copy_from_server,	NULL)
	PHP_FE(pssh_tasklist_add,	NULL)
	PHP_FE(pssh_tasklist_exec,	a3_arg_force_ref)
	PHP_FE(pssh_tasklist_first,	NULL)
	PHP_FE(pssh_tasklist_next,	NULL)
	PHP_FE(pssh_task_server_name,	NULL)
	PHP_FE(pssh_task_status,	NULL)
	PHP_FE(pssh_task_stderr,	NULL)
	PHP_FE(pssh_task_stdout,	NULL)
	PHP_FE(pssh_task_exit_status,	NULL)
	PHP_FE(pssh_task_type,	NULL)
	PHP_FE(pssh_task_get_cmd,	NULL)
	{NULL, NULL, NULL}
};
/* }}} */

/* {{{ pssh_module_entry
 */
zend_module_entry pssh_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"pssh",
	pssh_functions,
	PHP_MINIT(pssh),
	NULL,
	NULL,
	NULL,
	PHP_MINFO(pssh),
#if ZEND_MODULE_API_NO >= 20010901
	PHP_PSSH_VERSION,
#endif
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
