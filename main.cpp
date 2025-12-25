#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <sstream>
#include <signal.h>
#include <cstring>
#include<cstdint>
#include <sys/stat.h>
#include <fcntl.h>
#include "vfs.hpp"

void analyze_disk(const std::string& device_path)
{
    std::ifstream device(device_path, std::ios::binary);

    if (!device)
    {
        std::cout << "Error: Cannot open device " << device_path << " (need root?)\n";
        return;
    }

    char sector[512];
    device.read(sector, 512);

    if (device.gcount() != 512)
    {
        std::cout << "Error: Cannot read disk\n";
        return;
    }

    if ((unsigned char)sector[510] != 0x55 || (unsigned char)sector[511] != 0xAA)
    {
        std::cout << "Error: Invalid disk signature (not MBR/GPT)\n";
        return;
    }

    bool is_gpt = false;
    for (int i = 0; i < 4; i++)
    {
        if ((unsigned char)sector[446 + i * 16 + 4] == 0xEE)
        {
            is_gpt = true;
            break;
        }
    }

    std::cout << "Device: " << device_path << "\n";
    std::cout << "Type: " << (is_gpt ? "GPT" : "MBR") << "\n";

    if (!is_gpt)
    {
        std::cout << "MBR Partitions:\n";
        for (int i = 0; i < 4; i++)
        {
            int offset = 446 + i * 16;
            unsigned char type = sector[offset + 4];

            if (type != 0)
            {
                uint32_t num_sectors = *(uint32_t*)&sector[offset + 12];
                uint32_t size_mb = num_sectors / 2048; 
                bool bootable = ((unsigned char)sector[offset] == 0x80);

                std::cout << "  Partition " << (i + 1) << ": ";
                std::cout << "Size=" << size_mb << "MB, ";
                std::cout << "Type=0x" << std::hex << (int)type << std::dec << ", ";
                std::cout << "Bootable=" << (bootable ? "Yes" : "No") << "\n";
            }
        }
    }
    else
    {
        device.read(sector, 512);
        if (device.gcount() == 512 &&
            sector[0] == 'E' && sector[1] == 'F' && sector[2] == 'I' &&
            sector[3] == ' ' && sector[4] == 'P' && sector[5] == 'A' &&
            sector[6] == 'R' && sector[7] == 'T')
        {
            uint32_t num_partitions = *(uint32_t*)&sector[80];
            std::cout << "GPT Partitions: " << num_partitions << "\n";
        }
        else
        {
            std::cout << "GPT Partitions: unknown\n";
        }
    }
}
volatile sig_atomic_t g_sighup_received = 0;

void sighup_handler(int signal_number)
{
    if (signal_number == SIGHUP)
    {
        write(STDOUT_FILENO, "\nConfiguration reloaded\n", 24);
        g_sighup_received = 1;
    }
}

void execute_external(const std::vector<std::string>& tokens)
{
    if (tokens.empty()) return;

    pid_t pid = fork();

    if (pid == 0)
    {
        setenv("PATH", "/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin", 1);

        std::vector<char*> args;
        for (const auto& t : tokens)
        {
            args.push_back(const_cast<char*>(t.c_str()));
        }
        args.push_back(nullptr); 

        execvp(args[0], args.data());

        std::string explicit_paths[] = { "/bin/", "/usr/bin/", "/usr/local/bin/" };
        for (const auto& path_prefix : explicit_paths)
        {
            std::string full_path = path_prefix + tokens[0];
            execv(full_path.c_str(), args.data());
        }

        std::cerr << tokens[0] << ": command not found\n";
        exit(1);
    }
    else if (pid > 0)
    {
        int status;
        waitpid(pid, &status, 0);
    }
    else
    {
        std::cerr << "Failed to create process (fork error)\n";
    }
}

void print_env_variable(const std::string& var_name)
{
    const char* value = std::getenv(var_name.c_str());

    if (value != nullptr)
    {
        std::string value_str = value;

        if (value_str.find(':') != std::string::npos)
        {
            std::stringstream ss(value_str);
            std::string part;
            while (std::getline(ss, part, ':'))
            {
                if (!part.empty())
                {
                    std::cout << part << "\n";
                }
            }
        }
        else
        {
            std::cout << value_str << "\n";
        }
    }
    else
    {
        std::cout << var_name << ": не найдено\n";
    }
}

void show_command_history(const std::string& history_path)
{
    std::ifstream history_file(history_path);
    if (!history_file.is_open())
    {
        return;
    }

    std::string line;
    int line_number = 1;
    while (std::getline(history_file, line))
    {
        std::cout << line_number << "  " << line << "\n";
        line_number++;
    }
    history_file.close();
}

int main()
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    struct sigaction sa;
    sa.sa_handler = sighup_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;  

    if (sigaction(SIGHUP, &sa, NULL) == -1)
    {
        std::cerr << "Warning: Failed to set SIGHUP handler\n";
    }

    setenv("PATH", "/bin:/usr/bin:/usr/local/bin:/sbin:/usr/sbin", 0);

    const char* home = std::getenv("HOME");
    if (!home)
    {
        std::cerr << "ERROR: HOME environment variable not set\n";
        return 1;
    }

    std::string users_dir = std::string(home) + "/users";

    std::cout << "[INFO] Creating users VFS at: " << users_dir << "\n";

    start_users_vfs(users_dir);
    DIR* dir = opendir(users_dir.c_str());
    if (dir) {
        int file_count = 0;
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                file_count++;
            }
        }
        closedir(dir);

        if (file_count == 0) {
            cout << "[INFO] Creating sample user structure..." << endl;

            struct passwd* pw = getpwuid(getuid());
            if (pw) {
                string user_dir = users_dir + "/" + string(pw->pw_name);
                mkdir(user_dir.c_str(), 0755);

                ofstream(user_dir + "/id") << pw->pw_uid << endl;
                ofstream(user_dir + "/home") << pw->pw_dir << endl;
                ofstream(user_dir + "/shell") << pw->pw_shell << endl;

                cout << "[INFO] Created entry for user: " << pw->pw_name << endl;
            }
        }
    }


    std::string history_path = std::string(home) + "/.kubsh_history";
    std::string input;

    while (true)
    {
        if (g_sighup_received)
        {
            std::cout << "\nConfiguration reloaded\n";
            g_sighup_received = 0;
            std::cout << "$ ";
            fflush(stdout);
        }

        std::cout << "$ ";
        fflush(stdout);

        if (!std::getline(std::cin, input))
        {
            std::cout << "\n";
            break;
        }

        if (input.empty())
        {
            continue;
        }

        std::ofstream history_file(history_path, std::ios::app);
        if (history_file.is_open())
        {
            history_file << input << "\n";
            history_file.close();
        }

        if (input == "\\q")
        {
            break;
        }

        if (input == "history")
        {
            show_command_history(history_path);
            continue;
        }

        if (input.find("echo ") == 0)
        {
            std::string text = input.substr(5);
            std::string result = "";

            for (size_t i = 0; i < text.size(); i++)
            {
                if (text[i] == '$' && i + 1 < text.size())
                {
                    size_t start = i + 1;
                    size_t end = start;

                    while (end < text.size() &&
                        (isalnum(text[end]) || text[end] == '_'))
                    {
                        end++;
                    }

                    std::string var_name = text.substr(start, end - start);
                    const char* var_value = getenv(var_name.c_str());

                    if (var_value)
                    {
                        result += var_value;
                        i = end - 1;
                    }
                    else
                    {
                        result += text[i];
                    }
                }
                else
                {
                    result += text[i];
                }
            }

            std::cout << result << "\n";
            continue;
        }
        if (input.find("\\e $") == 0)
        {
            std::string var_name = input.substr(4);
            print_env_variable(var_name);
            continue;
        }

        if (input.substr(0, 3) == "\\l ")
        {
            std::string device = input.substr(3);
          
            device.erase(0, device.find_first_not_of(" \t"));
            device.erase(device.find_last_not_of(" \t") + 1);

            if (device.empty())
            {
                std::cout << "Usage: \\l /dev/device_name (e.g., \\l /dev/sda)\n";
            }
            else
            {
                analyze_disk(device);
            }
            continue;
        }

        if (input.size() >= 9 && input.substr(0, 7) == "debug '" && input.back() == '\'')
        {
            std::string text = input.substr(7, input.size() - 8);
            std::cout << text << "\n";
            continue;
        }

        std::vector<std::string> tokens;
        std::stringstream iss(input);
        std::string token;

        while (iss >> token)
        {
            tokens.push_back(token);
        }

        if (!tokens.empty())
        {
            execute_external(tokens);
        }
    }

    return 0;
}