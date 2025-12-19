#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <unistd.h>

#include <sys/wait.h>

int main() {
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::string home = "/home/vboxuser";
    std::string historyPath = home + "/.kubsh_history";

    std::string input;

    std::cout << "$ ";

    while (std::getline(std::cin, input)) {
        if (!input.empty()) {
            std::ofstream history(historyPath, std::ios::app);
            history << input << "\n";
            history.close();
        }

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
                std::cout << "Variable '" << var_name << "' not found\n";
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
        }
        else if (!input.empty()) {
            std::vector<std::string> tokens;
            std::istringstream iss(input);
            std::string token;

            while (iss >> token) {
                tokens.push_back(token);
            }

            if (tokens.empty()) {
                std::cout << "$ ";
                continue;
            }

            pid_t pid = fork();

            if (pid == 0) {
                std::vector<char*> args;
                for (auto& t : tokens) {
                    args.push_back(const_cast<char*>(t.c_str()));
                }
                args.push_back(nullptr);  

                execvp(args[0], args.data());

                std::cerr << tokens[0] << ": command not found\n";
                exit(1);
            }
            else if (pid > 0) {
                int status;
                waitpid(pid, &status, 0);  
            }
            else {
                std::cerr << " error when creating the process\n";
            }
        }

        std::cout << "$ ";
    }

}