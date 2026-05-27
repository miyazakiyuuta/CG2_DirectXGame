#include <string>  
#include <iostream>  
int main() { std::string s = reinterpret_cast<const char*>(u8"BGM‘I‘ð"); std::cout << s.size() << std::endl; return 0; }  
