#include <iostream>
#include <string>
#include <fstream>

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::string home = "/home/vboxuser";
    std::string historyPath = home + "/.kubsh_history";

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
        else if (input.find("\\e ") == 0) {
            std::string var_name = input.substr(3);
            bool show_as_list = false;

            if (var_name.find(" list") != std::string::npos) {
                show_as_list = true;
                var_name = var_name.substr(0, var_name.size() - 5);
            }

            std::string value;

            if (var_name == "HOME") {
                value = "/home/vboxuser";
            }
            else if (var_name == "PATH") {
                value = "/usr/local/bin:/usr/bin:/bin:/usr/games";
            }
            else if (var_name == "USER") {
                value = "vboxuser";
            }
            else if (var_name == "SHELL") {
                value = "/bin/bash";
            }
            else if (var_name == "PWD") {
                value = home;
            }
            else {
                std::cout << "Переменная '" << var_name << "' не найдена\n";

                std::ofstream history(historyPath, std::ios::app);
                history << input << "\n";
                history.close();

                std::cout << "$ ";
                continue;
            }

            if (show_as_list) {
                size_t start = 0;
                size_t end = value.find(':');

                while (end != std::string::npos) {
                    std::cout << value.substr(start, end - start) << "\n";
                    start = end + 1;
                    end = value.find(':', start);
                }
                std::cout << value.substr(start) << "\n";
            }
            else {
                std::cout << value << "\n";
            }

            std::ofstream history(historyPath, std::ios::app);
            history << input << "\n";
            history.close();
        }
        else if (!input.empty()) {
            std::cout << input << ": command not found\n";

            std::ofstream history(historyPath, std::ios::app);
            history << input << "\n";
            history.close();
        }

        std::cout << "$ ";
    }
}