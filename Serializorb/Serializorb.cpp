// Serializorb.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "serializorb.h"

#include <iostream>

struct test2 : public serializable {

    int myint;
    char char_name[20];
    std::string str_name;

    void serialize(serializer* s) override {
        
        s->serialize_field(myint);
        s->serialize_field(char_name);
        s->serialize_field(str_name);
    }
};

struct test : public serializable {

    double v;
    long long f;
    bool boolean;
    test2 innertest;


    void serialize(serializer* s) override {
        s->serialize_field(v);
        s->serialize_field(f);
        s->serialize_field(boolean);
        s->serialize_field(innertest);
    }
};

struct test3 : public test {

    int inheritedvalue;

    void serialize(serializer* s) override {
        test::serialize(s);

        s->serialize_field(inheritedvalue);
    }
};

int main()
{
    std::cout << "Hello World!\n";

    test3 t;
    //t.inheritedvalue = 43;
    //t.v = 2432.64;
    //t.f = 2429346298522;
    //t.boolean = true;

    //const char* str = "Hello World!";
    //strcpy_s(t.innertest.char_name, str);
    //t.innertest.myint = 235;
    //t.innertest.str_name = "Hello String";


    serializer_json s;

    s.start_read("test.json");
    t.serialize(&s);
    s.stop();

}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file



void serializer::enter_scope(std::string& name) {
    if (state() == SERIALIZE_WRITING) {
        enter_scope_impl(name);
        scopes.push({ name, 0 });
    }
    else {
        cur = cur->deep[name].get();
    }

}
void serializer::exit_scope() {
    if (state() == SERIALIZE_WRITING) {
        exit_scope_impl();
        scopes.pop();
    }
    else {
        cur = cur->parent;
    }

}

void serializer::start_read(std::string file_name) {
    _state = SERIALIZE_READING;
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


    map = std::make_unique<deserialized_map>();
    map->parent = nullptr; //should be set in constructor but just make sure incase of any weirdness
    cur = map.get();

    while (cur != nullptr) {

        cur = deserialize_impl(cur, ss);
    }

    //reset ready for read
    cur = map.get();
}

void serializer::start_write(std::string file_name) {
    _state = SERIALIZE_WRITING;
    file.open(file_name, std::fstream::out);
    scopes.push({ "",0 });
    file_write_start();
}

void serializer::stop() {
    if (state() == SERIALIZE_WRITING) {
        file_write_end();
        file.close();
    }
    else {
        map = nullptr;
    }

}



void serializer::serialize_deep(serializable* object, std::string& name) {

    enter_scope(name);
    object->serialize(this);
    exit_scope();
}
