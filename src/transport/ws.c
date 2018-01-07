/*+
 * Copyright 2015 Two Pore Guys, Inc.
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <glib.h>
#include <libsoup/soup.h>
#include "../linker_set.h"
#include "../internal.h"

#define	WS_MAX_MESSAGE_SIZE	(1024 * 1024) /* 1MB */

static gboolean ws_do_connect(gpointer user_data);
static int ws_connect(struct rpc_connection *, const char *, rpc_object_t);
static void ws_connect_done(GObject *, GAsyncResult *, gpointer);
static int ws_listen(struct rpc_server *, const char *, rpc_object_t);
static void ws_receive_message(SoupWebsocketConnection *, SoupWebsocketDataType,
    GBytes *, gpointer);
static int ws_send_message(void *, void *, size_t, const int *, size_t);
static void ws_process_banner(SoupServer *, SoupMessage *, const char *,
    GHashTable *, SoupClientContext *, gpointer);
static void ws_process_connection(SoupServer *, SoupWebsocketConnection *,
    const char *, SoupClientContext *, gpointer);
static void ws_error(SoupWebsocketConnection *, GError *, gpointer);
static void ws_close(SoupWebsocketConnection *, gpointer);
static int ws_abort(void *);
static int ws_get_fd(void *);
static void ws_release(void *);

struct rpc_transport ws_transport = {
	.name = "websocket",
	.schemas = {"ws", "wss", NULL},
	.connect = ws_connect,
	.listen = ws_listen
};

struct ws_connection
{
    	GMutex				wc_mtx;
    	GCond				wc_cv;
    	GError *			wc_connect_err;
	GError *			wc_last_err;
	GSocketClient *			wc_client;
	GSocketConnection *		wc_conn;
    	SoupSession *			wc_session;
	SoupURI *			wc_uri;
	SoupWebsocketConnection *	wc_ws;
	struct rpc_connection *		wc_parent;
};

struct ws_server
{
	struct rpc_server *		ws_server;
	SoupServer *			ws_soupserver;
    	const char *			ws_path;
};

static gboolean
ws_do_connect(gpointer user_data)
{
	SoupMessage *msg;
	struct ws_connection *conn = user_data;

	conn->wc_session = soup_session_new_with_options(
	    SOUP_SESSION_USE_THREAD_CONTEXT, TRUE, NULL);
	msg = soup_message_new_from_uri(SOUP_METHOD_GET, conn->wc_uri);
	soup_session_websocket_connect_async(conn->wc_session, msg, NULL, NULL,
	    NULL, &ws_connect_done, conn);

	return (false);
}

static int
ws_connect(struct rpc_connection *rco, const char *uri_string,
    rpc_object_t args __unused)
{
	struct ws_connection *conn = NULL;

	conn = g_malloc0(sizeof(*conn));
	g_mutex_init(&conn->wc_mtx);
	g_cond_init(&conn->wc_cv);
	conn->wc_uri = soup_uri_new(uri_string);
	conn->wc_parent = rco;

	g_main_context_invoke(rco->rco_mainloop, ws_do_connect, conn);
	g_mutex_lock(&conn->wc_mtx);
	while (conn->wc_connect_err == NULL && conn->wc_ws == NULL)
		g_cond_wait(&conn->wc_cv, &conn->wc_mtx);
	g_mutex_unlock(&conn->wc_mtx);

	if (conn->wc_connect_err != NULL)
		goto err;

	return (0);
err:
	rpc_set_last_gerror(conn->wc_connect_err);
	g_object_unref(conn->wc_session);
	g_free(conn);
	return (-1);
}

static void
ws_connect_done(GObject *obj, GAsyncResult *res, gpointer user_data)
{
	GError *err = NULL;
	SoupWebsocketConnection *ws;
	struct ws_connection *conn = user_data;
	struct rpc_connection *rco = conn->wc_parent;

	ws = soup_session_websocket_connect_finish(SOUP_SESSION(obj),
	    res, &err);
	if (err != NULL) {
		g_mutex_lock(&conn->wc_mtx);
		conn->wc_connect_err = err;
		g_cond_signal(&conn->wc_cv);
		g_mutex_unlock(&conn->wc_mtx);
		return;
	}

	soup_websocket_connection_set_max_incoming_payload_size(ws,
	    WS_MAX_MESSAGE_SIZE);

	rco->rco_send_msg = ws_send_message;
	rco->rco_abort = ws_abort;
	rco->rco_get_fd = ws_get_fd;
	rco->rco_arg = conn;
	rco->rco_release = ws_release;

	g_mutex_lock(&conn->wc_mtx);
	conn->wc_ws = ws;
	g_signal_connect(conn->wc_ws, "closed", G_CALLBACK(ws_close), conn);
	g_signal_connect(conn->wc_ws, "message", G_CALLBACK(ws_receive_message),
	    conn);
	g_cond_broadcast(&conn->wc_cv);
	g_mutex_unlock(&conn->wc_mtx);
}

static int
ws_listen(struct rpc_server *srv, const char *uri_str,
    rpc_object_t args __unused)
{
	GError *err = NULL;
	GSocketAddress *addr;
	SoupURI *uri;
	struct ws_server *server;
	int ret = 0;

	uri = soup_uri_new(uri_str);
	server = calloc(1, sizeof(*server));
	server->ws_path = g_strdup(uri->path);
	server->ws_server = srv;
	server->ws_soupserver = soup_server_new(
	    SOUP_SERVER_SERVER_HEADER, "librpc",
	    NULL);

	soup_server_add_handler(server->ws_soupserver, "/", ws_process_banner,
	    server, NULL);
	soup_server_add_websocket_handler(server->ws_soupserver,
	    server->ws_path, NULL, NULL, ws_process_connection, server, NULL);

	addr = g_inet_socket_address_new_from_string(uri->host, uri->port);
	soup_server_listen(server->ws_soupserver, addr, 0, &err);
	if (err != NULL) {
		errno = err->code;
		ret = -1;
		goto done;
	}

done:
	g_object_unref(addr);
	if (err != NULL) {
		rpc_set_last_gerror(err);
		g_error_free(err);
	}

	return (ret);
}

static void
ws_process_banner(SoupServer *ss __unused, SoupMessage *msg,
    const char *path __unused, GHashTable *query __unused,
    SoupClientContext *client __unused, gpointer user_data)
{
	struct ws_server *server = user_data;
	const char *resp = g_strdup_printf(
	    "<h1>Hello from librpc</h1>"
	    "<p>Please use WebSockets endpoint located at %s</p>",
	    server->ws_path);

	soup_message_set_status(msg, 200);
	soup_message_body_append(msg->response_body, SOUP_MEMORY_STATIC, resp, strlen(resp));
}

static void
ws_process_connection(SoupServer *ss __unused,
    SoupWebsocketConnection *connection, const char *path __unused,
    SoupClientContext *client __unused, gpointer user_data)
{
	rpc_connection_t rco;
	struct ws_server *server = user_data;
	struct ws_connection *conn;

	debugf("new connection");

	conn = g_malloc0(sizeof(*conn));
	conn->wc_ws = connection;

	rco = rpc_connection_alloc(server->ws_server);
	rco->rco_send_msg = &ws_send_message;
	rco->rco_abort = &ws_abort;
	rco->rco_get_fd = &ws_get_fd;
	rco->rco_arg = conn;
	conn->wc_parent = rco;
	server->ws_server->rs_accept(server->ws_server, rco);

	g_object_ref(conn->wc_ws);
	g_signal_connect(conn->wc_ws, "closed", G_CALLBACK(ws_close), conn);
	g_signal_connect(conn->wc_ws, "error", G_CALLBACK(ws_error), conn);
	g_signal_connect(conn->wc_ws, "message",
	    G_CALLBACK(ws_receive_message), conn);
}

static void
ws_receive_message(SoupWebsocketConnection *ws __unused,
    SoupWebsocketDataType type __unused, GBytes *message, gpointer user_data)
{
	struct ws_connection *conn = user_data;
	const void *data;
	size_t len;

	data = g_bytes_get_data(message, &len);
	debugf("received frame: addr=%p, len=%zu", data, len);
	conn->wc_parent->rco_recv_msg(conn->wc_parent, data, len, NULL, 0, NULL);
}

static void
ws_error(SoupWebsocketConnection *ws __unused, GError *error, gpointer user_data)
{
	struct ws_connection *conn = user_data;

	conn->wc_last_err = error;
}

static void
ws_close(SoupWebsocketConnection *ws __unused, gpointer user_data)
{
	struct ws_connection *conn = user_data;

	debugf("closed: conn=%p", conn);

	if (conn->wc_last_err != NULL)
		rpc_set_last_gerror(conn->wc_last_err);

	conn->wc_parent->rco_close(conn->wc_parent);
}

static int
ws_send_message(void *arg, void *buf, size_t len, const int *fds __unused,
    size_t nfds __unused)
{
	struct ws_connection *conn = arg;

	if (soup_websocket_connection_get_state(conn->wc_ws) != SOUP_WEBSOCKET_STATE_OPEN)
		return (-1);

	soup_websocket_connection_send_binary(conn->wc_ws, buf, len);
	return (0);
}

static int
ws_abort(void *arg)
{
	struct ws_connection *conn = arg;

	soup_websocket_connection_close(conn->wc_ws, 1000, "Going away");
	return (0);
}

static void
ws_release(void *arg)
{
	struct ws_connection *conn = arg;

	soup_uri_free(conn->wc_uri);
	g_object_unref(conn->wc_ws);
	g_object_unref(conn->wc_session);
	g_free(conn);
}

static int
ws_get_fd(void *arg)
{
	struct ws_connection *conn = arg;

	return (g_socket_get_fd(g_socket_connection_get_socket(conn->wc_conn)));
}

DECLARE_TRANSPORT(ws_transport);
