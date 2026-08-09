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


    serializer_json s("test.json");

    s.start_read();
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
