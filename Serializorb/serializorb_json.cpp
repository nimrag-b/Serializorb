#include "serializorb.h"

void print_map(deserialized_map* map) {
    for (auto& i : map->map) {
        std::cout << i.second->key << " : " << i.second->str_value << std::endl;
    }

    for (auto& i : map->deep) {
        std::cout << i.first << " : {" << std::endl;
        print_map(i.second.get());
        std::cout << "}" << std::endl;
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

	if (serialized->flags & SERIALIZED_ARRAY_TYPE) {
		file << "[ ";
		for (size_t i = 0; i < serialized->size; i++)
		{
			if (i != 0) {
				file << ", ";
			}
			auto ptr = serialized->str_array();
			file << serialized->str_array();
			ptr += strlen(ptr) + 1;
		}
		file << " ]";
	}
	//dont need to wrap ints in quotes
	else if (serialized->flags & SERIALIZED_INT_TYPE || serialized->flags & SERIALIZED_FLOAT_TYPE) {
		file << serialized->str_value;
	}
	else {
		file << "\"" << serialized->str_value << "\"";
	}
}



deserialized_map* serializer_json::deserialize_impl(deserialized_map* map, std::stringstream& ss) {
	std::string name;
	std::string value;

	std::string ln;

	char ch = ss.peek();
	if (ch == '}') { //close object
		ss.get(); //eat
		return map->exit();
	}

	std::getline(ss, ln, '"'); //get up to start of next token
	std::getline(ss, name, '"'); //get name

	ch = ss.get(); // should be :

	ch = ss.get();
	if (ch == '{') { //new object
		return map->enter(name);
	}
	else if (ch == '[') { //array
		std::getline(ss, value, ']'); //get value
		throw new std::exception("NOT IMPLEMENTED");
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

	map->add(name, value);
	return map;
}
