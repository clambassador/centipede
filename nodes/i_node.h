#ifndef __IB__WEB__I_NODE__H__
#define __IB__WEB__I_NODE__H__

#include <sstream>
#include <string>
#include <vector>

#include "ib/abstract_property_page.h"
#include "ib/logger.h"
#include "centipede/types.h"

using namespace std;
using namespace ib;

namespace centipede {

class INode {
public:
	virtual ~INode() {}
	virtual string display(const ClientID& cid) = 0;
	virtual string display(AbstractPropertyPage* app) = 0;
	virtual void display(AbstractPropertyPage* app, stringstream* ss) = 0;
	virtual void set_name(const string& name) = 0;
	virtual const string& name() = 0;
	virtual void handle_command(const ClientID& cid,
				    int state,
				    const string& command,
				    AbstractPropertyPage* args) = 0;
	virtual void clear_style() = 0;
	virtual void set_style(const string&) = 0;
};

}  // namespace centipede

#endif  // __IB__WEB__I_NODE__H__
