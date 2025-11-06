#include <iostream>
#include <string>
#include <fstream>

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    const char* home = std::getenv("HOME");
    std::string historyPath = std::string(home) + "/.kubsh_history";

    std::string input;

    std::cout << "$ ";

    while (std::getline(std::cin, input)) {
        if (input == "\\q") {
            break;
        }
        else if (input == "history") {

            std::ifstream historyOutput(historyPath);
            std::string line;
            while (std::getline(historyOutput, line)) {
                std::cout << line << "\n";
            }
            historyOutput.close();
        }
        else if (input.find("echo ") == 0) {
            
            std::string text = input.substr(5);

            std::cout << text << "\n";


            std::ofstream history(historyPath, std::ios::app);
            history << input << "\n";
            history.close();
        }

        else if (!input.empty()) {
            std::ofstream history(historyPath, std::ios::app);
            history << input << "\n";
            history.close();
        }

        std::cout << "$ ";
    }

}