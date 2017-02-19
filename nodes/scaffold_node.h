#ifndef __CENTIPEDE__SCAFFOLD_NODE__H__
#define __CENTIPEDE__SCAFFOLD_NODE__H__

#include <cassert>
#include <cstring>
#include <sstream>
#include <string>

#include "ib/abstract_property_page.h"
#include "ib/logger.h"
#include "centipede/nodes/base_node.h"
#include "centipede/nodes/string_node.h"

using namespace std;

namespace centipede {

class ScaffoldNode : public StringNode {
public:
	ScaffoldNode() : ScaffoldNode("") {}
	ScaffoldNode(const string& file) : StringNode("") {
		if (!file.empty()) load_file(file);
	}

	virtual void display(AbstractPropertyPage* app,
			     stringstream* ss) {
		bool to_fill = false;
		for (auto &x: _pieces) {
			if (to_fill) {
				/*if (x[0] == '@') {
					Nodes::_()->get_node(
						x.substr(1))->display(app, ss);
				} else*/
				if (app->has(x)) *ss << app->get(x);
				else *ss << "%%" << x << "%%";
			} else *ss << x;
			to_fill = !to_fill;
		}
	}

        virtual string display(AbstractPropertyPage* app) {
                stringstream ss;
                display(app, &ss);
                return ss.str();
        }

	virtual void load_file(const string& file) {
		ifstream fin(file);
		assert(fin.good());
		fin.seekg(0, ios::end);
		size_t length = fin.tellg();
		fin.seekg(0, ios::beg);
		unique_ptr<char[]> buf(new char[length + 1]);
		Logger::info("(scaffold_node) file % has len %", file, length);
		fin.read(buf.get(), length);
		buf.get()[length] = 0;
		fin.close();
		char* save_ptr;
		char* token;
		char* str = buf.get();
		while ((token = strtok_r(str, "%%", &save_ptr))) {
			_pieces.push_back(token);
			str = nullptr;
		}
	}

	virtual void clear() {
		_text = "";
		_pieces.clear();
	}

protected:
	vector<string> _pieces;
};

}  // namespace centipede

#endif  // __CENTIPEDE__STRING_NODE__H__
