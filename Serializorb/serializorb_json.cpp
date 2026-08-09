#include "serializorb.h"

void print_map(deserialized_map* map) {
    for (auto& i : map->map) {
        std::cout << i.second->key << " : " << i.second->value << std::endl;
    }

    for (auto& i : map->deep) {
        std::cout << i.first << " : {" << std::endl;
        print_map(i.second);
        std::cout << "}" << std::endl;
    }
}

void serializer_json::start_read() {
	serializer::start_read();

    file.open(file_name, std::fstream::in);

    std::stringstream ss;
    std::string buf;
    while (std::getline(file, buf)) {
        ss << buf;
    }
    buf = ss.str();
    buf.erase(std::remove_if(buf.begin(), buf.end(), std::isspace), buf.end());

    file.close();

    ss = std::stringstream(buf);

    std::stack<deserialized_map*> map_stack;

    map = new deserialized_map;
    cur = map;

    map_stack.push(map);
    char ch = ss.get(); //should be {
    while (!map_stack.empty()) {
        char peek = ss.peek();
        if (peek == '}') {
            ss.get();
            map_stack.pop();
            continue;
        }

        deserialize(map_stack, ss);
    }

    //print_map(map);
}

void serializer_json::start_write() {
	serializer::start_write();
    file.open(file_name, std::fstream::out);
    scopes.push({ "",0 });
    file << "{";
}

void serializer_json::stop() {
    if (state == SERIALIZE_WRITING) {
        file << "\n}";
        file.close();
    }
    else {
        delete_map(map);
        map = nullptr;
    }

}

void serializer_json::enter_scope_impl(std::string& name) {
	if (scopes.top().second != 0) {
		file << ',';
	}
	file << "\n\"" << name << "\": " << "{";
}

void serializer_json::exit_scope_impl() {
	file << "\n}";
}

void serializer_json::serialize_impl(serialized_object* serialized) {

	if (scopes.top().second != 0) {
		file << ',';
	}

	file << "\n\"" << serialized->key << "\": ";
	//dont need to wrap ints in quotes
	if (serialized->flags & SERIALIZED_INT_TYPE || serialized->flags & SERIALIZED_FLOAT_TYPE) {
		file << serialized->value;
	}
	else {
		file << "\"" << serialized->value << "\"";
	}
}



void serializer_json::deserialize(std::stack<deserialized_map*>& map_stack, std::stringstream& ss) {
	std::string name;
	std::string value;

	std::string ln;
	std::getline(ss, ln, '"'); //get up to start of next token
	std::getline(ss, name, '"'); //get name

	char ch = ss.get(); // should be :

	ch = ss.get();
	if (ch == '{') { //new object
		deserialized_map* new_map = new deserialized_map;
		new_map->parent = map_stack.top();
		map_stack.top()->add(name, new_map);
		map_stack.push(new_map);
		return;
	}
	else if (ch == '\"') {
		std::getline(ss, value, '"'); //get value
	}
	else if (isalpha(ch)) { //quick and dirty parsing bool values
		std::stringstream ss1;
		while (isalpha(ch)) {
			ss.get();
			ss1 << ch;
			ch = ss.peek();
		}
		value = ss1.str();
		if (value == "true") {
			value = "1";
		}
		else if (value == "false") {
			value = "0";
		}
	}
	else { //number value
		std::stringstream ss1;
		while (isdigit(ch) || ch == '.') {
			ss.get();
			ss1 << ch;
			ch = ss.peek();
		}
		value = ss1.str();
	}

	serialized_object* object = new serialized_object;
	object->key = name;
	object->value = value;
	map_stack.top()->add(object);
}
