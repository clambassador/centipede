#ifndef __CENTIPEDE__STRING_NODE__H__
#define __CENTIPEDE__STRING_NODE__H__

#include <cassert>
#include <sstream>
#include <string>

#include "centipede/nodes/base_node.h"
#include "centipede/nodes/i_node.h"

using namespace std;

namespace centipede {

class StringNode : public BaseNode {
public:
	StringNode() : StringNode("") {}
	StringNode(const string& init) { _text = init; }

	template<typename... Args>
	StringNode(const string& format, Args... args) {
		_text = format;
		fold(args...);
	}
protected:
	template<typename... Args>
	void fold(INode* node, Args... args) {
		_args.push_back(node);
		fold(args...);
	}

	void fold() {}

public:

	virtual ~StringNode() {}
	virtual void set(const string& text) {
		_text = text;
	}

	virtual void display(AbstractPropertyPage* app, stringstream* ss) {
		assert(ss);
		if (_style.empty())
			display_text(app, ss);
		else {
			stringstream is;
			is << _style;
			string token;
			vector<string> tokens;
			while (is.good()) {
				is >> token;
				*ss << "<" + token + ">";
				tokens.push_back(token);
			}
			display_text(app, ss);
			while (!tokens.empty()) {
				token = tokens.back();
				*ss << "</" + token + ">";
				tokens.pop_back();
			}

		}
	}

	virtual void set_style(const string& style) {
		_style = style;
	}

protected:

	void display_text(AbstractPropertyPage* app, stringstream* ss) {
		auto x = _args.begin();
		const char* format = _text.c_str();
		while (*format) {
			if (*format == '%') {
				if (*(format + 1) != '%') {
					(*x++)->display(app, ss);
				} else {
					*ss << "%";
					++format;
				}
				++format;
			} else *ss << *format++;
		}
	}

	string _text;
	string _style;
	vector<INode*> _args;
};

}  // namespace centipede

#endif  // __CENTIPEDE__STRING_NODE__H__
