#ifndef __IB__WEB__WEBSERVER__H__
#define __IB__WEB__WEBSERVER__H__

#include <arpa/inet.h>
#include <cassert>
#include <dirent.h>
#include <cstdlib>
#include <cstring>
#include <map>
#include <microhttpd.h>
#include <set>
#include <string>
#include <vector>

#include "ib/config.h"
#include "ib/entropy.h"
#include "ib/logger.h"
#include "ib/tiny_timer.h"
#include "centipede/backend/i_webserver_backend.h"

#define POST_BUFFER_SIZE 1024

using namespace ib;
using namespace std;

namespace centipede {

static int http_serv(void * cls,
                     struct MHD_Connection * connection,
                     const char * url,
                     const char * method,
                     const char * version,
                     const char * upload_data,
                     size_t * upload_data_size,
                     void ** ptr);

static void request_completed(void* cls,
			      struct MHD_Connection *connection,
			      void **con_cls,
			      enum MHD_RequestTerminationCode toe);

class WebServer {
public:
	WebServer(IWebserverBackend* backend)
		: _alive(false), _backend(backend) {}

	void start_server(int port) {
		assert(port);
		_daemon = MHD_start_daemon(
			MHD_USE_THREAD_PER_CONNECTION,
			// MHD_USE_SELECT_INTERNALLY, is the alternate
			port,
			nullptr,
			nullptr,
			&http_serv,
			(void *) this,
			MHD_OPTION_NOTIFY_COMPLETED,
			&request_completed,
			nullptr,
			MHD_OPTION_END);
		if (_daemon == nullptr) {
			Logger::error("Failure to create http server "
			 	      "on port %", port);

			assert(0);
		}
		_alive = true;
		_housekeeping_thread.reset(new thread(
			&WebServer::housekeeping_thread, this));
	}

	void stop_server() {
		{
			unique_lock<mutex> ul(_mutex);
			if (!_alive) return;
			_alive = false;
		}
		_housekeeping_thread->join();
		MHD_stop_daemon(_daemon);
	}

	int recv_post(const string& url,
		      const string& key, const string& filename,
                      const string& content_type, const string& encoding,
                      const string& data, uint64_t offset,
                      size_t size, string* output) {
		vector<string> pieces;
		split_url(url, &pieces);
		if (!pieces.size()) {
			Logger::error("recv_post(): no pieces in %", url);
			throw "invalid request";
		}
		ClientID cid = atoll(pieces[0].c_str());

		unique_lock<mutex> ul(_mutex);
		if (!is_client(cid)) {
			build_redirect(output);
			return 0;
		}
		client_alive(cid);

		if (pieces.size() < 3 || pieces[1] != "command") {
			Logger::error("recv_post(): bad url %", url);
			throw "invalid request";
		}
		int finished = _backend->recv_post(
			cid, pieces[2], key, filename,
			content_type, encoding, data,
			offset, size, output);
		return MHD_YES;
		return finished ? MHD_YES : MHD_NO;
	}

	virtual bool log_connection(struct MHD_Connection* conn,
				    const string& url,
				    const string& method,
				    string* output) {
		char ipbuf[INET6_ADDRSTRLEN + 1];
		struct sockaddr_in* addr = * (struct sockaddr_in**)
			MHD_get_connection_info(
				conn,
				MHD_CONNECTION_INFO_CLIENT_ADDRESS);

		inet_ntop(addr->sin_family,
			  &addr->sin_addr,
			  ipbuf, INET6_ADDRSTRLEN);
		ipbuf[INET6_ADDRSTRLEN] = 0;
/*		Logger::info("(webserver) %:% requests %",
			ipbuf, ntohs(addr->sin_port),
			url);*/
		return true;
	}

	bool early_abort(const string& url, string* output) {
		vector<string> pieces;
		split_url(url, &pieces);
		if (!pieces.size() || pieces[0][0] == '?') {
			unique_lock<mutex> ul(_mutex);
			ClientID cid = new_session();
			_state[cid] = 0;
			client_alive(cid);
			build_output(cid, output);
			return true;
		}
		stringstream ss;
		ss << pieces[0];
		ClientID cid;
		ss >> cid;
		unique_lock<mutex> ul(_mutex);
		if (!is_client(cid)) {
			build_redirect(output);
			return true;
		}
		return false;
	}

	void raw_url(const string& url, string* output) const {
		if (url.find("..") != string::npos) {
			Logger::error("security violation: %", url);
			build_redirect(output);
			return;
		}
		stringstream ss;
		ifstream fin(url);
		ss << fin.rdbuf();
		if (ss.str().length() == 0) {
			Logger::error("raw_url(): empty data for %", url);
			*output = "<html><body>[data not available]</body></html>";
		} else {
			*output = ss.str();
		}
	}

	int geturl(const string& url, const map<string, string>& args,
		   string* output) {
		// TinyTimer tt("geturl");
		unique_lock<mutex> ul(_mutex);
		assert(output);
		vector<string> pieces;
		split_url(url, &pieces);
		if (!pieces.size()) {
			Logger::error("geturl() % % pieces empty",
				      url, args);
			throw "no client";
		}

		stringstream ss;
		ss << pieces[0];
		ClientID cid;
		ss >> cid;

		if (!is_client(cid)) {
			Logger::error("geturl(): % not client", cid);
			throw "unknown client";
		}

		client_alive(cid);
		/* hostname/cid */
		if (pieces.size() == 1) {
			build_output(cid, output);
			return 0;
		}

		if (pieces[1] == "get") {
			if (pieces.size() < 3) {
				Logger::error("geturl(): % % not enough "
					      "parameters", url, args);
				throw "invalid request";
			}
                        vector<string> arguments;
                        string key = pieces[2];
			int i = 3;
			while (pieces.size() > i) {
                                arguments.push_back(pieces[i]);
                                ++i;
                        }
			_backend->get_value(cid, _state[cid],
					    key, arguments, args, output);
			return 0;
		}
		if (pieces[1] == "set") {
			if (pieces.size() < 3) {
				Logger::error("geturl(): % % not enough "
					      "parameters", url, args);
				throw "invalid request";
			}
                        vector<string> arguments;
                        string key = pieces[2];
			int i = 3;
			while (pieces.size() > i) {
                                arguments.push_back(pieces[i]);
                                ++i;
                        }
			if (_backend->set_value(cid, _state[cid],
					    key, arguments, args)) {
				*output = "";
			} else {
				*output = "error";
			}
			return 0;
		}
		if (pieces[1] == "resource") {
			if (pieces.size() < 3) {
				Logger::error("geturl(): % % not enough "
                                             "parameters", url, args);
                                throw "invalid request";
			}
			ResourceID rid = atoll(pieces[2].c_str());
			string ject = "";
			if (pieces.size() == 4) ject = pieces[3];
			_backend->get_resource(cid, rid, ject, output);
			return 0;
		}

		if (pieces[1] == "command" || pieces[1] == "call") {
			if (pieces.size() < 3) {
				Logger::error("geturl(): % % not enough "
                                             "parameters", url, args);
                                throw "invalid request";
			}
			vector<string> arguments;
			string command = pieces[2];
			int i = 3;
			if (command == "for_a_node") i += 2;

			while (pieces.size() > i) {
				arguments.push_back(pieces[i]);
				++i;
			}
			if (command == "for_a_node") {
				_backend->run_node_command(
					cid, _state[cid],
					pieces[3], pieces[4], arguments, args);
			} else {
				_state[cid] = _backend->run_command(
					cid, _state[cid],
					command, arguments, args);
			}
			if (pieces[1] == "call") *output = "";
			else build_output(cid, output);
			return 0;
		}
		Logger::error("url prefix % not found.", pieces[1]);
		return -1;
	}

	virtual void build_redirect(string* output) const {
		*output = "<!DOCTYPE html><html lang=en><head><title>"
		          "redirect</title><script>function redirect() {"
			  "window.location.pathname=\"/?random=" +
			  Logger::stringify(entropy::_<uint64_t>())
			  + "\";}\n</script></head><body onload=redirect()>beep</body></html>";
	}

protected:
	virtual void build_output(const ClientID& cid, string* output) {
		_backend->get_page(cid, _state[cid], output);
		security_checks(cid, output);
	}

	virtual void security_checks(const ClientID& cid, string* output) {
		stringstream ss;
		/* todo: perhaps bulid this list by querying the root abstract
		 * node. but what is the best way to achieve this in general
		 * without having a specific query name for tallying different
		 * aspects.
		 */
	}

	bool is_client(const ClientID& cid) const {
		return _state.count(cid);
	}

	ClientID new_session() {
		ClientID cid;
		do {
			cid = entropy::_<uint64_t>();
		} while (_clients.count(cid));
		_clients.insert(cid);
		_backend->new_client(cid);
		// TODO: rate limting
		return cid;
	}

	void split_url(const string& url, vector<string> *pieces) const {
		assert(pieces);
                size_t len = url.length();
                char* buf = (char *) malloc(sizeof(char) * len);
                strncpy(buf, url.c_str() + 1, len - 1);
                buf[len - 1] = 0;

                char *p = strtok(buf, "/");
                while (p) {
			if (strlen(p)) pieces->push_back(string(p));
                        p = strtok(NULL, "/");
                }
                free(buf);
        }

	void client_alive(const ClientID& cid) {
		_last_active[cid] = sensible_time::runtime();
	}

	void housekeeping_thread() {
		chrono::milliseconds
			milliseconds(Config::_()->get("housekeeping_timeout_ms"));
		int tidy_period = Config::_()->get("tidy_period");
		int life_period = Config::_()->get("life_period");
		int last_tidied = sensible_time::runtime();
		set<ClientID> evict_list;

		while (_alive) {
			this_thread::sleep_for(milliseconds);
			unique_lock<mutex> ul(_mutex);

			if (sensible_time::runtime() - last_tidied > tidy_period) {
				last_tidied = sensible_time::runtime();
				for (auto &x : _last_active) {
					if (sensible_time::runtime() - x.second
					    > life_period) {
						Logger::info("(housekeeping) "
							     "I'm done with %, "
							     "been idle for % "
							     "seconds.",
							     x.first,
							     sensible_time::runtime()
							     - x.second);
						evict_list.insert(x.first);
					}
				}
				continue;
			}
			if (!evict_list.empty()) {
				evict_client(*evict_list.begin());
				evict_list.erase(*evict_list.begin());
			}
		}
	}

	virtual void evict_client(const ClientID& cid) {
		Logger::info("(housekeeping) byebye %", cid);
		_backend->bye_client(cid);
		_state.erase(cid);
		_last_active.erase(cid);
		_clients.erase(cid);
		_possible_commands.erase(cid);
		_possible_resources.erase(cid);
	}

	mutex _mutex;
	set<ClientID> _clients;
	unique_ptr<thread> _housekeeping_thread;
	bool _alive;

	IWebserverBackend* _backend;
	struct MHD_Daemon * _daemon;
	map<ClientID, int> _state;
	map<ClientID, int> _last_active;

	map<ClientID, set<string>> _possible_commands;
	map<ClientID, set<string>> _possible_resources;
};

struct connection_info_struct
{
	string answer;
	string url;
	struct MHD_PostProcessor *post_processor;
	WebServer* webserver;
};

static void request_completed(void* cls,
			      struct MHD_Connection *connection,
			      void **con_cls,
			      enum MHD_RequestTerminationCode toe) {
	struct connection_info_struct* con_info =
		(struct connection_info_struct *) *con_cls;
	if (!con_info) return;
	MHD_destroy_post_processor(con_info->post_processor);
	free(con_info);
	*con_cls = nullptr;
}

static int iterate_post(
		void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
		const char *filename, const char *content_type,
		const char *transfer_encoding, const char *data, uint64_t off,
		size_t size) {
	struct connection_info_struct* con_info =
		(struct connection_info_struct*) coninfo_cls;
	Logger::info("iterate post: % % off % len %", key,
		     filename ? filename : "", off, size);
	return con_info->webserver->recv_post(
		con_info->url, key ? key : "", filename ? filename : "",
		content_type ? content_type : "",
		transfer_encoding ? transfer_encoding : "",
		data ? string(data, size) : "", off, size, &(con_info->answer));
}

static int send_page(struct MHD_Connection *connection,
		     const string& output) {
	struct MHD_Response* response = MHD_create_response_from_buffer(
		output.length(),
		(void *) output.c_str(),
		MHD_RESPMEM_MUST_COPY);
	int ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
	//Logger::info("(webserver) reply length %", output.length());
	return ret;
}

static int add_map_cb(void *cls,
		       enum MHD_ValueKind kind,
		       const char *key, const char *value) {
	map<string, string>* m = reinterpret_cast<map<string, string>*>(cls);
	(*m)[string(key)] = string(value);
	return MHD_YES;
}

static int http_serv(void * cls,
                     struct MHD_Connection * connection,
                     const char * url,
                     const char * method,
                     const char * version,
                     const char * upload_data,
                     size_t * upload_data_size,
                     void ** ptr) {
	string output;
	WebServer* webserver = static_cast<WebServer*>(cls);
	if (!webserver->log_connection(connection, url, method, &output)) {
		return send_page(connection, output);
	}
	if (string(url) == "/favicon.ico") return MHD_NO;

	try {

	if (strncmp("/raw__", url, 6) == 0) {
		webserver->raw_url(url + 1, &output);
		return send_page(connection, output);
	}
	if (webserver->early_abort(url, &output)) {
		return send_page(connection, output);
	}

	if (string(method) == "POST") {
		if (!*ptr) {  // new post connection
			struct connection_info_struct *con_info =
				new struct connection_info_struct();
			if (!con_info) return MHD_NO;
			con_info->webserver = webserver;
			con_info->post_processor =
				MHD_create_post_processor(connection,
							  POST_BUFFER_SIZE,
							  iterate_post,
							  (void *) con_info);
			if (!con_info->post_processor) {
				delete con_info;
				return MHD_NO;
			}
			if (*upload_data_size) {
				Logger::error("for % had upload %",
					      url,
					      *upload_data_size);
				throw "invalid argument";
			}
			*ptr = (void *) con_info;
			return MHD_YES;
		}

		struct connection_info_struct *con_info =
			static_cast<struct connection_info_struct *>(*ptr);
		if (*upload_data_size != 0) {
			con_info->url = url;
			MHD_post_process(con_info->post_processor,
					 upload_data,
					 *upload_data_size);
			*upload_data_size = 0;
		} else {
			webserver->recv_post(url, "", "", "", "", "", 0, 0,
					     nullptr);
			/* HERE: run the post command, get the url */
			string output;
			webserver->geturl(string(url), map<string, string>(), &output);
			return send_page(connection, output);

		}
		//
		//	if (!con_info->answer.empty()) {
		//	return send_page(connection, con_info->answer);
		//}
		return MHD_YES;
	}

	if (string(method) != "GET") return MHD_NO;

	static int dummy;
	if (&dummy != *ptr) {
		*ptr = &dummy;
		return MHD_YES;
	}
	if (*upload_data_size) return MHD_NO;
	*ptr = nullptr;

	// TODO: pass useful information from connection

	map<string, string> args;
	MHD_get_connection_values(
		connection,
		MHD_GET_ARGUMENT_KIND,
		&add_map_cb,
		&args);

	webserver->geturl(string(url), args, &output);
	return send_page(connection, output);

	} catch (string s) {
		Logger::error("(webserver) caught exception \"%\".", s);
		webserver->build_redirect(&output);
		return send_page(connection, output);
	}
}

}  // namespace centipede

#endif  // __IB__WEB__WEBSERVER__H__
