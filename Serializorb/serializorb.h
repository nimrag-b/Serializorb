#pragma once
#include <string>
#include <sstream>
#include <stack>
#include <iostream>
#include <fstream> 
#include <unordered_map>
#include <array>
#include <variant>
#include <ranges>


namespace detail
{
	// To allow ADL with custom begin/end
	using std::begin;
	using std::end;

	template <typename T>
	auto is_iterable_impl(int)
		-> decltype (
			begin(std::declval<T&>()) != end(std::declval<T&>()), // begin/end and operator !=
			void(), // Handle evil operator ,
			++std::declval<decltype(begin(std::declval<T&>()))&>(), // operator ++
			void(*begin(std::declval<T&>())), // operator*
			std::true_type{});

	template <typename T>
	std::false_type is_iterable_impl(...);


}
template <typename T>
using is_iterable = decltype(detail::is_iterable_impl<T>(0));

template <typename T>
constexpr bool is_iterable_v = is_iterable<T>::value;

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
	SERIALIZED_ARRAY_TYPE = 1 << 3,
};


struct serialized_object {

	std::string key;

	std::unique_ptr<std::string> str_value = nullptr;
	std::unique_ptr<std::vector<uint8_t>> bin_value = nullptr;

	const char* str_array() const {
		return reinterpret_cast<const char*>(bin_value->data());
	}
	uint8_t* bin_array() const {
		return bin_value->data();
	}

	uint8_t size = 0; 
	uint8_t flags = 0;
};


struct deserialized_map {


	deserialized_map* parent = nullptr;
	std::unordered_map<std::string, std::unique_ptr<serialized_object>> map;
	std::unordered_map<std::string, std::unique_ptr<deserialized_map>> deep;


	deserialized_map* enter(std::string name) {

		deserialized_map* child = new deserialized_map;
		child->parent = this;
		deep.insert({ name,std::unique_ptr<deserialized_map>(child) });
		return child;
	}
	deserialized_map* exit() {
		return parent;
	}


	void add(std::string name, std::string& value) {

		serialized_object* object = new serialized_object;
		object->key = name;
		object->str_value = std::make_unique<std::string>(value);

		map.insert({ object->key, std::unique_ptr<serialized_object>(object) });
	}


	void add(std::string name, std::vector<uint8_t>& value) {

		serialized_object* object = new serialized_object;
		object->key = name;
		object->bin_value = std::make_unique<std::vector<uint8_t>>(value);

		map.insert({ object->key, std::unique_ptr<serialized_object>(object) });
	}

private:
	bool is_binary;
};

#define serialize_field(object) _serialize_field(&object, #object)
constexpr bool SERIALIZE_READING = true;
constexpr bool SERIALIZE_WRITING = false;


template<typename T>
inline std::vector<uint8_t> to_binary_vec(T* object) {
	std::array< uint8_t, sizeof(T) > bytes;

	const uint8_t* begin = reinterpret_cast<const uint8_t*>(object);
	const uint8_t* end = begin + sizeof(T);
	std::copy(begin, end, std::begin(bytes));

	return std::vector<uint8_t>(bytes.begin(), bytes.end());
}
template<typename T>
inline T from_binary_vec(std::vector<uint8_t>& vec) {
	T object;
	void* dst = reinterpret_cast<void*>(std::addressof(object));
	std::memcpy(dst, vec.data(), sizeof(T));

	return object;
}

class serializer {
protected:
	std::fstream file;
	std::stack<std::pair<std::string, int>> scopes;
	std::unique_ptr<deserialized_map> map;
	deserialized_map* cur = nullptr; //points towards somewhere in map





	virtual void file_write_start() = 0;
	virtual void file_write_end() = 0;


	virtual void enter_scope_impl(std::string& name) = 0;
	virtual void exit_scope_impl() = 0;

	//saves serialized type to file
	virtual void serialize_impl(serialized_object* serialized) = 0;
	virtual deserialized_map* deserialize_impl(deserialized_map* map, std::stringstream& ss) = 0;


	void enter_scope(std::string& name);
	void exit_scope();

public:

	serializer(bool binary) {
		_is_binary = binary;
	}

	const bool state() const {
		return _state;
	}
	const bool is_binary() const {
		return _is_binary;
	}

	void start_read(std::string file_name);
	void start_write(std::string file_name);
	void stop();


	template<typename T>
	void _serialize_field(T* object, std::string name) {

		static_assert(!std::is_pointer_v<T>, "cannot serialize pointer"); //cant be pointer


		if constexpr (std::is_base_of_v<serializable, T>) { // if not a generic type, only serializable objects allowed
			serialize_deep(object, name);
		}
		else  {
			serialized_object serialized_type;

			if (state() == SERIALIZE_WRITING) {
				serialize_type(&serialized_type, object, name);
				serialize_impl(&serialized_type);
				scopes.top().second++; //increment count
			}
			else {

				deserialize_type(cur->map.at(name).get(), object);
			}


		}
	}


private:
	bool _state = false;
	bool _is_binary = false;
	void serialize_deep(serializable* object, std::string& name);



	template<typename T>
	void serialize_type(serialized_object* serialized, T* object, std::string& name) {

		get_serialized_type<T>(serialized);
		serialized->key = name;

		if constexpr (std::is_convertible_v<T, std::string>) { //is string

			if constexpr (std::is_base_of_v<std::string, T>) {

				if (is_binary()) {
					serialized->bin_value = std::make_unique<std::vector<uint8_t>>(object->begin(), object->end());
				}
				else {
					serialized->str_value = std::make_unique<std::string>(*object);

				}


			}
			else {
				if (is_binary()) {

					serialized->bin_value = std::make_unique <std::vector<uint8_t>>(*object, *object + sizeof(T));
				}
				else {
					serialized->str_value = std::make_unique<std::string>(*object);
				}

			}
		}
		else if constexpr (is_iterable_v<T>) {

			if (is_binary()) {

				serialized->bin_value = std::make_unique<std::vector<uint8_t>>();
				auto it = std::begin(*object);
				auto end_it = std::end(*object);

				while (it != end_it)
				{
					auto v = *it;
					std::vector<uint8_t> tmp = to_binary_vec(&v);
					serialized->bin_value->insert(serialized->bin_value->end(), tmp.begin(),tmp.end());
					++it;
				}
				serialized->size = std::size(*object);
			}
			else { //store as null terminated substrings 

				serialized->bin_value = std::make_unique<std::vector<uint8_t>>();
				auto it = std::begin(*object);
				auto end_it = std::end(*object);
				while (it != end_it)
				{
					auto v = *it;
					std::vector<uint8_t> tmp = to_binary_vec(&v);

					std::string s = std::to_string(v);
					serialized->bin_value->insert(serialized->bin_value->end(), s.begin(), s.end());
					serialized->bin_value->push_back(0); //null terminator
					++it;
				}
				serialized->size = std::size(*object);
			}
		}
		else {

			static_assert(std::is_fundamental_v<T>, "cannot serialize non fundamental type");

			if (is_binary()) {

				serialized->bin_value = std::make_unique<std::vector<uint8_t>>(to_binary_vec(object));
			}
			else {

				serialized->str_value = std::make_unique<std::string>(std::to_string(*object));
			}

		}


	}


	template<typename T>
	void deserialize_type(serialized_object* serialized, T* object) {

		if constexpr (std::is_convertible_v<T, std::string>) { //is string

			if constexpr (std::is_base_of_v<std::string, T>) {

				if (is_binary()) {
					*object = std::string(serialized->bin_value->begin(),serialized->bin_value->end());
				}
				else {
					*object = *serialized->str_value.get();
				}


			}
			else {
				if (is_binary()) {
					memcpy((void*)*object, serialized->bin_value->data(), sizeof(T));
				}
				else {
					memcpy((void*)*object, serialized->str_value->c_str(), sizeof(T));
				}

			}


		}
		else if constexpr (is_iterable_v<T>) {
			if (is_binary()) {

				auto ptr = serialized->bin_value->data();
				typename T::value_type t;

				for (size_t i = 0; i < serialized->size; i++)
				{
					std::memcpy(&t, ptr + (i * sizeof(t)), sizeof(t));

					object->insert(object->end(), t);
				}

			}
			else { //store as null terminated substrings 


				auto ptr = serialized->str_array();
				typename T::value_type t;
				for (size_t i = 0; i < serialized->size; i++)
				{
					size_t len = strlen(ptr) + 1;

					std::istringstream ss(ptr);
					ss >> t;

					ptr += len;
					
					object->insert(object->end(), t);
				}
			}
		}
		else {
			static_assert(std::is_fundamental_v<T> == true, "cannot deserialize non fundamental type");

			if (is_binary()) {

				*object = from_binary_vec<T>(*serialized->bin_value.get());
			}
			else {
				std::istringstream ss(serialized->str_array());
				ss >> *object;
			}

		}

	}





	template<typename T>
	inline void get_serialized_type(serialized_object* serialized) {

		serialized->flags = 0;
		if constexpr (std::is_convertible_v<T, std::string>) { //is string

			serialized->size = 0;
			serialized->flags |= SERIALIZED_STRING_TYPE;
		}
		else if constexpr (is_iterable_v<T>) {
			serialized->size = 0;
			serialized->flags |= SERIALIZED_ARRAY_TYPE;

		}
		else {
			static_assert(std::is_fundamental_v<T> == true, "cannot serialize non fundamental type");
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

public:
	serializer_json(): serializer(false) {

	}

	void file_write_start() override {
		file << "{";
	}

	void file_write_end() override {
		file << "\n}";
	}


private:
	void enter_scope_impl(std::string& name) override;
	void exit_scope_impl() override;
	void serialize_impl(serialized_object* serialized) override;
	deserialized_map* deserialize_impl(deserialized_map* map, std::stringstream& ss) override;
};