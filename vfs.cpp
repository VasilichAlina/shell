#include "vfs.hpp"
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>

using namespace std;

void start_users_vfs(const string& mount_point) {
    cout << "[VFS] Creating directory: " << mount_point << endl;

    // 1. Создаем директорию
    mkdir(mount_point.c_str(), 0755);

    // 2. Получаем всех пользователей системы
    vector<string> users;

    ifstream passwd("/etc/passwd");
    string line;

    while (getline(passwd, line)) {
        stringstream ss(line);
        vector<string> parts;
        string part;

        while (getline(ss, part, ':')) {
            parts.push_back(part);
        }

        if (parts.size() >= 7) {
            string username = parts[0];
            string uid = parts[2];

            // Берем только обычных пользователей (не системных)
            try {
                int uid_num = stoi(uid);
                if (uid_num >= 1000 && uid_num < 60000) {
                    users.push_back(username);
                }
            }
            catch (...) {
                continue;
            }
        }
    }

    // 3. Создаем подкаталоги для каждого пользователя
    for (const auto& username : users) {
        string user_dir = mount_point + "/" + username;
        mkdir(user_dir.c_str(), 0755);

        // Получаем информацию о пользователе
        struct passwd* pw = getpwnam(username.c_str());
        if (pw) {
            // Создаем файл id
            ofstream id_file(user_dir + "/id");
            id_file << pw->pw_uid << endl;
            id_file.close();

            // Создаем файл home
            ofstream home_file(user_dir + "/home");
            home_file << pw->pw_dir << endl;
            home_file.close();

            // Создаем файл shell
            ofstream shell_file(user_dir + "/shell");
            shell_file << pw->pw_shell << endl;
            shell_file.close();

            cout << "[VFS] Created: " << username << endl;
        }
    }

    // 4. Если нет пользователей, создаем тестового с помощью adduser
    if (users.empty()) {
        cout << "[VFS] No regular users found, creating testuser..." << endl;

        // Создаем пользователя через adduser
        string cmd = "sudo adduser --disabled-password --gecos '' testuser 2>/dev/null";
        int result = system(cmd.c_str());

        if (result == 0) {
            string user_dir = mount_point + "/testuser";
            mkdir(user_dir.c_str(), 0755);

            ofstream(user_dir + "/id") << "1001" << endl;
            ofstream(user_dir + "/home") << "/home/testuser" << endl;
            ofstream(user_dir + "/shell") << "/bin/bash" << endl;

            cout << "[VFS] Created testuser with adduser" << endl;
        }
    }

    cout << "[VFS] Ready. Total users: " << users.size() << endl;
    cout << "[VFS] Check: ls " << mount_point << endl;
}

void stop_users_vfs() {
    cout << "[VFS] Cleaning up..." << endl;

    // Удаляем тестового пользователя если создавали
    string cmd = "sudo userdel -r testuser 2>/dev/null";
    system(cmd.c_str());

    cout << "[VFS] Stopped" << endl;
}