/*
   +----------------------------------------------------------------------+
   | PHP version 4.0                                                      |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997, 1998, 1999, 2000 The PHP Group                   |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | http://www.php.net/license/2_01.txt.                                 |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Jayakumar Muthukumarasamy <jk@kasenna.com>                   |
   +----------------------------------------------------------------------+
*/

/*
 * PHP includes
 */
#include "php.h"

#include "ext/standard/info.h"

#include "php_ini.h"
#include "php_globals.h"
#include "SAPI.h"
#include "main.h"
#include "php_version.h"

/*
 * NSAPI includes
 */
#include "nsapi.h"
#include "base/pblock.h"
#include "base/session.h"
#include "frame/req.h"
#include "frame/protocol.h"  /* protocol_start_response */
#include "base/util.h"       /* is_mozilla, getline */
#include "frame/log.h"       /* log_error */

/*
 * Timeout for net_read(). This should probably go into php.ini
 */
#define NSAPI_READ_TIMEOUT	60	/* 60 seconds */

#define NSLS_D		struct nsapi_request_context *request_context
#define NSLS_DC		, NSLS_D
#define NSLS_C		request_context
#define NSLS_CC		, NSLS_C
#define NSG(v)		(request_context->v)

/*
 * Currently, this doesn't work with ZTS.
 */
#if defined(ZTS)
	#define IF_ZTS(a)	a
	#define IF_NOT_ZTS(a)
#else
	#define IF_ZTS(a)
	#define IF_NOT_ZTS(a)	a
	static CRITICAL php_mutex;
#endif

/*
 * Structure to encapsulate the NSAPI request in SAPI
 */
typedef struct nsapi_request_context {
	pblock	*pb;
	Session	*sn;
	Request	*rq;
} nsapi_request_context;

/*
 * Mappings between NSAPI names and environment variables. This
 * mapping was obtained from the sample programs at the iplanet
 * website.
 */
typedef struct nsapi_equiv {
	const char *env_var;
	const char *nsapi_eq;
} nsapi_equiv;

static nsapi_equiv nsapi_headers[] = {
	{ "CONTENT_LENGTH",		"content-length" },
	{ "CONTENT_TYPE",		"content-type" },
	{ "HTTP_ACCEPT",		"accept" },
	{ "HTTP_ACCEPT_ENCODING",	"accept-encoding" },
	{ "HTTP_ACCEPT_LANGUAGE",	"accept-language" },
	{ "HTTP_AUTHORIZATION",		"authorization" },
	{ "HTTP_COOKIE",		"cookie" },
	{ "HTTP_IF_MODIFIED_SINCE",	"if-modified-since" },
	{ "HTTP_REFERER",		"referer" },
	{ "HTTP_USER_AGENT",		"user-agent" },
	{ "HTTP_USER_DEFINED",		"user-defined" }
};
static size_t nsapi_headers_size = sizeof(nsapi_headers)/sizeof(nsapi_headers[0]);

static nsapi_equiv nsapi_reqpb[] = {
	{ "QUERY_STRING",		"query" },
	{ "REQUEST_LINE",		"clf-request" },
	{ "REQUEST_METHOD",		"method" },
	{ "SCRIPT_NAME",		"uri" },
	{ "SCRIPT_PROTOCOL",		"protocol" }
};
static size_t nsapi_reqpb_size = sizeof(nsapi_reqpb)/sizeof(nsapi_reqpb[0]);

static nsapi_equiv nsapi_vars[] = {
	{ "AUTH_TYPE",			"auth-type" },
	{ "PATH_INFO",			"path-info" },
	{ "REMOTE_USER",		"auth-user" }
};
static size_t nsapi_vars_size = sizeof(nsapi_vars)/sizeof(nsapi_vars[0]);

static nsapi_equiv nsapi_client[] = {
	{ "HTTPS_KEYSIZE",		"keysize" },
	{ "HTTPS_SECRETSIZE",		"secret-keysize" },
	{ "REMOTE_ADDR",		"ip" }
};
static size_t nsapi_client_size = sizeof(nsapi_client)/sizeof(nsapi_client[0]);

static int
sapi_nsapi_ub_write(const char *str, unsigned int str_length)
{
	int retval;
	nsapi_request_context *rc;

	SLS_FETCH();
	rc = (nsapi_request_context *)SG(server_context);
	retval = net_write(rc->sn->csd, str, str_length);
	if (retval == IO_ERROR /*-1*/ || retval == IO_EOF /*0*/)
		return -1;
	else
		return retval;
}

static int
sapi_nsapi_header_handler(sapi_header_struct *sapi_header, sapi_headers_struct *sapi_headers SLS_DC)
{
	char *header_name, *header_content, *p;
	nsapi_request_context *rc = (nsapi_request_context *)SG(server_context);

	header_name = sapi_header->header;
	header_content = p = strchr(header_name, ':');
	if (p == NULL) {
		return 0;
	}

	*p = 0;
	do {
		header_content++;
	} while (*header_content==' ');

	if (!strcasecmp(header_name, "Content-Type")) {
		param_free(pblock_remove("content-type", rc->rq->srvhdrs));
		pblock_nvinsert("content-type", header_content, rc->rq->srvhdrs);
	} else if (!strcasecmp(header_name, "Set-Cookie")) {
		pblock_nvinsert("set-cookie", header_content, rc->rq->srvhdrs);
	} else {
		pblock_nvinsert(header_name, header_content, rc->rq->srvhdrs);
	}

	*p = ':';	/* restore '*p' */

	efree(sapi_header->header);

	return 0;	/* don't use the default SAPI mechanism, NSAPI duplicates this functionality */
}

static int
sapi_nsapi_send_headers(sapi_headers_struct *sapi_headers SLS_DC)
{
	int retval;
	nsapi_request_context *rc = (nsapi_request_context *)SG(server_context);

	/*
	 * We could probably just do this in the header_handler. But, I
	 * don't know what the implication of doing it there is.
	 */
	if (SG(sapi_headers).send_default_content_type) {
		param_free(pblock_remove("content-type", rc->rq->srvhdrs));
		pblock_nvinsert("content-type", "text/html", rc->rq->srvhdrs);
	}

	protocol_status(rc->sn, rc->rq, SG(sapi_headers).http_response_code, NULL);
	retval = protocol_start_response(rc->sn, rc->rq);
	if (retval == REQ_PROCEED || retval == REQ_NOACTION)
		return SAPI_HEADER_SENT_SUCCESSFULLY;
	else
		return SAPI_HEADER_SEND_FAILED;
}

static int
sapi_nsapi_read_post(char *buffer, uint count_bytes SLS_DC)
{
	nsapi_request_context *rc = (nsapi_request_context *)SG(server_context);
	char *read_ptr = buffer, *content_length_str = NULL;
	uint bytes_read = 0;
	int length, content_length = 0;
	netbuf *nbuf = rc->sn->inbuf;

	/*
	 * Determine the content-length. This will tell us the limit we can read.
	 */
	content_length_str = pblock_findval("content-length", rc->rq->headers);
	if (content_length_str != NULL) {
		content_length = strtol(content_length_str, 0, 0);
	}

	if (content_length <= 0)
		return 0;

	/*
	 * Gobble any pending data in the netbuf.
	 */
	length = nbuf->cursize - nbuf->pos;
	if (length > 0) {
		memcpy(read_ptr, nbuf->inbuf + nbuf->pos, length);
		bytes_read += length;
		read_ptr += length;
		content_length -= length;
	}

	/*
	 * Read the remaining from the socket.
	 */
	while (content_length > 0 && bytes_read < count_bytes) {
		int bytes_to_read = count_bytes - bytes_read;
		if (content_length < bytes_to_read)
			bytes_to_read = content_length;

		length = net_read(rc->sn->csd, read_ptr, bytes_to_read, NSAPI_READ_TIMEOUT);
		if (length == IO_ERROR || length == IO_EOF)
			break;

		bytes_read += length;
		read_ptr += length;
		content_length -= length;
	}

	return bytes_read;
}

static char *
sapi_nsapi_read_cookies(SLS_D)
{
	char *cookie_string;
	nsapi_request_context *rc = (nsapi_request_context *)SG(server_context);

	cookie_string = pblock_findval("cookie", rc->rq->headers);
	return cookie_string;
}

static sapi_module_struct nsapi_sapi_module = {
	"NSAPI",				/* name */

	php_module_startup,			/* startup */
	php_module_shutdown_wrapper,		/* shutdown */

	NULL,					/* activate */
	NULL,					/* deactivate */

	sapi_nsapi_ub_write,			/* unbuffered write */
	NULL,					/* flush */
	NULL,					/* get uid */
	NULL,					/* getenv */

	php_error,				/* error handler */

	sapi_nsapi_header_handler,		/* header handler */
	sapi_nsapi_send_headers,		/* send headers handler */
	NULL,					/* send header handler */

	sapi_nsapi_read_post,			/* read POST data */
	sapi_nsapi_read_cookies,		/* read Cookies */

	NULL,					/* register server variables */
	NULL,					/* Log message */

	NULL,					/* Block interruptions */
	NULL,					/* Unblock interruptions */

	STANDARD_SAPI_MODULE_PROPERTIES
};

static char *
nsapi_strdup(char *str)
{
	if (str != NULL)
		return strdup(str);
	return NULL;
}

static void
nsapi_free(void *addr)
{
	if (addr != NULL)
		free(addr);
}

/*
 * Add symbols to the interpreter.
 */
static void
nsapi_add_string(const char *name, const char *buf)
{
	zval *pval;
	ELS_FETCH();

	if (buf == NULL)
		buf = "";

	MAKE_STD_ZVAL(pval);
	pval->type = IS_STRING;
	pval->value.str.len = strlen(buf);
	pval->value.str.val = estrndup(buf, pval->value.str.len);
	zend_hash_update(&EG(symbol_table), name, strlen(name) + 1, &pval, sizeof(zval *), NULL);
}

static void
nsapi_hash_environment(NSLS_D SLS_DC)
{
	size_t i;
	const char *remote_host = NULL, *server_url = NULL, *path_translated = NULL;
	char *value = NULL, buf[128];

	remote_host = session_dns(NSG(sn));
	server_url = http_uri2url("", "");
	path_translated = SG(request_info).path_translated;

	*buf = 0;

	for (i = 0; i < nsapi_headers_size; i++) {
		if ((value = pblock_findval(nsapi_headers[i].nsapi_eq, NSG(rq)->headers)) == NULL) {
			value = buf;
		}
		nsapi_add_string(nsapi_headers[i].env_var, value);
	}

	for (i = 0; i < nsapi_reqpb_size; i++) {
		if ((value = pblock_findval(nsapi_reqpb[i].nsapi_eq, NSG(rq)->reqpb)) == NULL) {
			value = buf;
		}
		nsapi_add_string(nsapi_reqpb[i].env_var, value);
	}

	for (i = 0; i < nsapi_vars_size; i++) {
		if ((value = pblock_findval(nsapi_vars[i].nsapi_eq, NSG(rq)->vars)) == NULL) {
			value = buf;
		}
		nsapi_add_string(nsapi_vars[i].env_var, value);
	}

	for (i = 0; i < nsapi_client_size; i++) {
		if ((value = pblock_findval(nsapi_client[i].nsapi_eq, NSG(sn)->client)) == NULL) {
			value = buf;
		}
		nsapi_add_string(nsapi_client[i].env_var, value);
	}

	sprintf(buf, "%d", conf_getglobals()->Vport);
	nsapi_add_string("SERVER_PORT", buf);

	nsapi_add_string("HTTPS", (security_active ? "ON" : "OFF"));
	nsapi_add_string("SERVER_NAME", server_hostname);
	nsapi_add_string("REMOTE_HOST", remote_host);
	nsapi_add_string("SERVER_URL", server_url);
/*	nsapi_add_string("SERVER_SOFTWARE", MAGNUS_VERSION_STRING); */
	nsapi_add_string("PATH_TRANSLATED", path_translated);
}

static void
nsapi_request_ctor(NSLS_D SLS_DC)
{
	char *query_string = pblock_findval("query", NSG(rq)->reqpb);
	char *uri = pblock_findval("uri", NSG(rq)->reqpb);
	char *path_info = pblock_findval("path-info", NSG(rq)->vars);
	char *path_translated = NULL;
	char *request_method = pblock_findval("method", NSG(rq)->reqpb);
	char *content_type = pblock_findval("content-type", NSG(rq)->headers);
	char *content_length = pblock_findval("content-length", NSG(rq)->headers);

	if (uri != NULL)
		path_translated = request_translate_uri(uri, NSG(sn));

#if 0
	log_error(LOG_INFORM, "nsapi_request_ctor", NSG(sn), NSG(rq),
		"query_string = %s, "
		"uri = %s, "
		"path_info = %s, "
		"path_translated = %s, "
		"request_method = %s, "
		"content_type = %s, "
		"content_length = %s",
		query_string,
		uri,
		path_info,
		path_translated,
		request_method,
		content_type,
		content_length);
#endif

	SG(request_info).query_string = nsapi_strdup(query_string);
	SG(request_info).request_uri = nsapi_strdup(path_info);
	SG(request_info).request_method = nsapi_strdup(request_method);
	SG(request_info).path_translated = nsapi_strdup(path_translated);
	SG(request_info).content_type = nsapi_strdup(content_type);
	SG(request_info).content_length = (content_length == NULL) ? 0 : strtoul(content_length, 0, 0);
}

static void
nsapi_request_dtor(NSLS_D SLS_DC)
{
	nsapi_free(SG(request_info).query_string);
	nsapi_free(SG(request_info).request_uri);
	nsapi_free(SG(request_info).request_method);
	nsapi_free(SG(request_info).path_translated);
	nsapi_free(SG(request_info).content_type);
}

static int
nsapi_module_main(NSLS_D SLS_DC)
{
	int result;
	zend_file_handle file_handle;

	CLS_FETCH();
	ELS_FETCH();
	PLS_FETCH();

	if (php_request_startup(CLS_C ELS_CC PLS_CC SLS_CC) == FAILURE) {
		return FAILURE;
	}

	file_handle.type = ZEND_HANDLE_FILENAME;
	file_handle.filename = SG(request_info).path_translated;
	file_handle.free_filename = 0;

#if 0
	log_error(LOG_INFORM, "nsapi_module_main", NSG(sn), NSG(rq),
		"Parsing [%s]", SG(request_info).path_translated);
#endif

	result = php_request_startup(CLS_C ELS_CC PLS_CC SLS_CC);
	if (result == FAILURE) {
		return FAILURE;
	}

	nsapi_hash_environment(NSLS_C SLS_CC);
	php_execute_script(&file_handle CLS_CC ELS_CC PLS_CC);
	php_request_shutdown(NULL);

	return SUCCESS;
}

int
php4_init(pblock *pb, Session *sn, Request *rq)
{
	PLS_FETCH();

	/*
	 * TSRM has not been tested.
	 */
	IF_ZTS(
		tsrm_startup(1, 1, 0);
	)

	sapi_startup(&nsapi_sapi_module);
	sapi_module.startup(&nsapi_sapi_module);

	PG(expose_php) = 0;

	IF_NOT_ZTS(
		php_mutex = crit_init();
	)

	log_error(LOG_INFORM, "php4_init", sn, rq, "Initialized PHP Module\n");
	return REQ_PROCEED;
}

int
php4_execute(pblock *pb, Session *sn, Request *rq)
{
	int retval;
	nsapi_request_context *request_context;

	SLS_FETCH();

	request_context = (nsapi_request_context *)malloc(sizeof(nsapi_request_context));
	request_context->pb = pb;
	request_context->sn = sn;
	request_context->rq = rq;

	/*
	 * Single thread the execution, if ZTS is not enabled. Need to
	 * understand the behavior of Netscape server when the client
	 * cancels a request when it is in progress. This could cause
	 * a deadlock if the thread handling the specific client is not
	 * cancelled because the php_mutex will likely remain locked
	 * until the request that was cancelled completes. The behavior
	 * is also going to be platform specific.
	 */
	IF_NOT_ZTS(
		crit_enter(php_mutex);
	)

	SG(server_context) = request_context;

	nsapi_request_ctor(NSLS_C SLS_CC);
	retval = nsapi_module_main(NSLS_C SLS_CC);
	nsapi_request_dtor(NSLS_C SLS_CC);

	IF_NOT_ZTS(
		crit_exit(php_mutex);
	)

	free(request_context);

	return (retval == SUCCESS) ? REQ_PROCEED : REQ_EXIT;
}
