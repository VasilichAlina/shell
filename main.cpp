#include <iostream>
#include <string>


int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::string input;


    while (std::getline(std::cin, input)) {

        if (input == "\\q") {
            exit;  
        }


        std::cout << input << "\n"; 
        std::cout << "$ ";
    }


}