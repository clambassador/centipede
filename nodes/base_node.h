#ifndef __IB__WEB__BASE_NODE__H__
#define __IB__WEB__BASE_NODE__H__

#include <cassert>
#include <sstream>
#include <string>

#include "ib/base_property_page.h"
#include "centipede/nodes/i_node.h"
#include "centipede/types.h"

using namespace std;

namespace centipede {

class BaseNode : public INode {
public:
	BaseNode() : BaseNode("") {}
	BaseNode(const string& text) : _text(text) {}
	virtual ~BaseNode() {}
	virtual void set_text(const string& text) {
		_text = text;
	}
	virtual void set_name(const string& name) {
		_name = name;
	}
	virtual const string& name() {
		return _name;
	}
	virtual void handle_command(const ClientID& cid,
				    int state,
				    const string& command,
				    AbstractPropertyPage* args) {
		assert(0);
	}

	virtual string display(const ClientID& cid) {
		BasePropertyPage bpp;
		bpp.set("cid", cid);
		return display(&bpp);
	}

	virtual string display(AbstractPropertyPage* app) {
		stringstream ss;
		display(app, &ss);
		return ss.str();
	}

	virtual void display(AbstractPropertyPage* app, stringstream* ss) {
		assert(0);
	}

        virtual void clear_style() {
		assert(0);
	}

	virtual void set_style(const string&) {
		assert(0);
	}

protected:
	string _name;
	string _text;
};

}  // namespace centipede

#endif  // __IB__WEB__BASE_NODE__H__
