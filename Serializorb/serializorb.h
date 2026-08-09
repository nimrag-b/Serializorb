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



	void delete_map(deserialized_map* map);

	void enter_scope(std::string& name);

	void exit_scope();

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

	void serialize_deep(serializable* object, std::string& name);



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
			map = nullptr;
		}
	}

	void start_read() override;

	void start_write() override;

	void stop() override;


private:
	void enter_scope_impl(std::string& name) override;
	void exit_scope_impl() override;
	void serialize_impl(serialized_object* serialized) override;

	void deserialize(std::stack<deserialized_map*>& map_stack, std::stringstream& ss);
};