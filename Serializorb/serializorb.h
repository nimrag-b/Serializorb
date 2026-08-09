#pragma once
#include <string>
#include <sstream>
#include <stack>
#include <iostream>
#include <fstream> 
#include <unordered_map>

class serializer;

class serializable {
public:
	virtual void serialize(serializer* s) = 0;
};


enum SERIALIZE_FLAGS
{
	SERIALIZED_INT_TYPE = 1 << 0,
	SERIALIZED_FLOAT_TYPE = 1 << 1,
	SERIALIZED_STRING_TYPE = 1 << 2,
};

struct serialized_object {
	std::string key;
	std::string value;
	uint8_t size; 
	uint8_t flags;
};


struct deserialized_map {
	deserialized_map* parent;
	std::unordered_map<std::string, serialized_object*> map;
	std::unordered_map<std::string, deserialized_map*> deep;

	void add(serialized_object* object) {
		map.insert({ object->key, object });
	}
	void add(std::string name, deserialized_map* map) {
		deep.insert({ name, map });
	}
};

#define serialize_field(object) _serialize_field(&object, #object)
constexpr bool SERIALIZE_READING = true;
constexpr bool SERIALIZE_WRITING = false;


class serializer {
protected:
	std::stack<std::pair<std::string, int>> scopes;
	deserialized_map* map;
	deserialized_map* cur;

	virtual void start_read() {
		state = SERIALIZE_READING;
	}
	virtual void start_write() {
		state = SERIALIZE_WRITING;
	}
	virtual void stop() = 0;

	virtual void enter_scope_impl(std::string& name) = 0;
	virtual void exit_scope_impl() = 0;

	//saves serialized type to file
	virtual void serialize_impl(serialized_object* serialized) = 0;

	//loads value of template object from file
	//virtual void deserialize_impl(serialized_object* template_object) = 0;

	void enter_scope(std::string& name) {
		if (state == SERIALIZE_WRITING) {
			enter_scope_impl(name);
			scopes.push({ name, 0 });
		}
		else {
			cur = cur->deep[name];
		}

	}

	void exit_scope() {
		if (state == SERIALIZE_WRITING) {
			exit_scope_impl();
			scopes.pop();
		}
		else {
			cur = cur->parent;
		}

	}

	bool state;

public:

	template<typename T>
	void _serialize_field(T* object, std::string name) {

		static_assert(!std::is_pointer_v<T>, "cannot serialize pointer"); //cant be pointer


		if constexpr (std::is_base_of_v<serializable, T>) { // if not a generic type, only serializable objects allowed
			serialize_deep(object, name);
		}
		else  {
			serialized_object serialized_type;

			if (state == SERIALIZE_WRITING) {
				serialize_type(&serialized_type, object, name);
				serialize_impl(&serialized_type);
				scopes.top().second++; //increment count
			}
			else {

				std::string value = cur->map.at(name)->value;
				deserialize_type(value, object);
			}


		}
	}
private:

	void serialize_deep(serializable* object, std::string& name) {

		enter_scope(name);
		object->serialize(this);
		exit_scope();
	}



	template<typename T>
	void serialize_type(serialized_object* serialized, T* object, std::string& name) {

		if constexpr (std::is_convertible_v<T, std::string>) { //is string

			get_serialized_type<T>(serialized);

			serialized->key = name;
			serialized->value = *object;
		}
		else {

			static_assert(std::is_fundamental_v<T> == true, "cannot serialize non fundamental type");

			get_serialized_type<T>(serialized);

			serialized->key = name;
			serialized->value = std::to_string(*object);
		}


	}


	template<typename T>
	void deserialize_type(std::string& value, T* object) {

		if constexpr (std::is_convertible_v<T, std::string>) { //is string
			if constexpr (std::is_base_of_v<std::string,T>) {

				*object = value;
			}
			else {
				memcpy((void*)*object, value.c_str(), sizeof(T));
			}

		}
		else {
			static_assert(std::is_fundamental_v<T> == true, "cannot deserialize non fundamental type");

			std::istringstream ss(value);
			ss >> *object;
		}

	}





	template<typename T>
	inline void get_serialized_type(serialized_object* serialized) {

		if constexpr (std::is_convertible_v<T, std::string>) { //is string
			serialized->flags = 0;
			serialized->size = 0;
			serialized->flags |= SERIALIZED_STRING_TYPE;
		}
		else {
			static_assert(std::is_fundamental_v<T> == true, "cannot serialize non fundamental type");
			serialized->flags = 0;
			serialized->size = sizeof(T);
			if constexpr (std::is_integral_v<T>) {
				serialized->flags |= SERIALIZED_INT_TYPE;
			}
			if constexpr (std::is_floating_point_v<T>) {
				serialized->flags |= SERIALIZED_FLOAT_TYPE;
			}
		}



	}


};

class serializer_json : public serializer {

	std::string file_name;
	std::fstream file;
public:
	serializer_json(std::string file) : file_name(file) {
		map = nullptr;
	}

	~serializer_json() {
		if (state == SERIALIZE_READING && map != nullptr) {
			delete_map(map);
		}
	}

	void start_read() override {
		state = SERIALIZE_READING;
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

			deserialize(map_stack,ss);
		}
		
		print_map(map);
	}

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

	void start_write() override {
		state = SERIALIZE_WRITING;
		file.open(file_name, std::fstream::out);
		scopes.push({ "",0 });
		file << "{";
	}

	void stop() override {
		if (state == SERIALIZE_WRITING) {
			file << "\n}";
			file.close();
		}
		else {
			delete_map(map);
			map = nullptr;
		}

	}

	void delete_map(deserialized_map* map) {
		for (auto& m : map->deep) {
			delete_map(m.second);
		}
		delete map;
	}
private:
	void enter_scope_impl(std::string& name) override {
		if (scopes.top().second != 0) {
			file << ',';
		}
		file << "\n\"" << name << "\": " << "{";
	}
	void exit_scope_impl() override {
		file << "\n}";
	}
	void serialize_impl(serialized_object* serialized) override {

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

	void deserialize(std::stack<deserialized_map*>& map_stack, std::stringstream& ss) {
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

		serialized_object *object = new serialized_object;
		object->key = name;
		object->value = value;
		map_stack.top()->add(object);
	}
};