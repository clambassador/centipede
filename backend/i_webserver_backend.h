#ifndef __CENTIPEDE__I_WEBSERVER_BACKEND__H__
#define __CENTIPEDE__I_WEBSERVER_BACKEND__H__

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "centipede/types.h"

using namespace std;

namespace centipede {

/* AbstractWebserverBackend is a virtual class that is used by the WebServer.
 * Each program using a built-in webserver is required to create its own
 * implementaiton of this virtual class and provide an instance to the
 * webserver. This abstraction provides the interface for the webserver
 * to operate without specific knowledge of the application.
 */
class IWebserverBackend {
public:
	virtual ~IWebserverBackend() {}

	/* get_page: takes the client ID and current state and places the
		     html output in the string parameter. */
	virtual void get_page(const ClientID&, int state,
			      string* output) = 0;

	/* get_resource: takes the client ID and resource ID and places the
			 raw resource in the string parameter. It throws an
			 exception if the client is not authorized for the
			 resource. */
	virtual void get_resource(const ClientID&,
				  const ResourceID&,
				  const string& ject,
				  string* output) = 0;

	virtual bool get_value(const ClientID&, int state,
                               const string& name,
                               const vector<string>& parameters,
                               const map<string, string>& arguments,
			       string* output) = 0;

	virtual bool set_value(const ClientID&, int state,
                               const string& name,
                               const vector<string>& parameters,
                               const map<string, string>& arguments) = 0;

	/* run_command: takes the client ID, current state, the name of the
	 * 		command, and a vector of arguments to that command. It
	 * 		returns the new state after running the command
	 */
	virtual int run_command(const ClientID&, int state,
				const string& command,
				const vector<string>& parameters,
				const map<string, string>& arguments) = 0;

	/* run_node_command: takes the client ID, current state, the name of the
	 * 		     the node, and a vector of arguments to send it. It
	 *		     sends the arguments to the named node.
	 */
	virtual void run_node_command(const ClientID&, int state,
				      const string& node,
				      const string& command,
				      const vector<string>& parameters,
				      const map<string, string>& arguments) = 0;

	virtual int recv_post(const ClientID&, const string& command,
			      const string& key, const string& filename,
			      const string& content_type, const string& encoding,
			      const string& data, uint64_t offset,
			      size_t size, string* output) = 0;

	/* new_client: informs the backend that a new client has begun a
	 * session.
	 */
	virtual void new_client(const ClientID&) = 0;

	/* bye_client: informs the backend that a client has ended a session. */
	virtual void bye_client(const ClientID&) = 0;
};

}  // namespace centipede

#endif  // __CENTIPEDE__I_WEBSERVER_BACKEND__H__
