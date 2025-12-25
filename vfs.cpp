#include "vfs.hpp"
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <cstdlib>
#include <vector>
#include <sstream>

using namespace std;

void start_users_vfs(const string& mount_point) {
    cout << "[VFS] Creating users directory: " << mount_point << endl;

    // 1. Создаем основную директорию
    system(("mkdir -p " + mount_point).c_str());

    // 2. Получаем список пользователей из /etc/passwd
    ifstream passwd("/etc/passwd");
    string line;
    int count = 0;

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
            string homedir = parts[5];
            string shell = parts[6];

            // Пропускаем системных пользователей (UID < 1000)
            try {
                int uid_num = stoi(uid);
                if (uid_num >= 1000) {
                    string user_dir = mount_point + "/" + username;

                    // Создаем директорию пользователя
                    system(("mkdir -p " + user_dir).c_str());

                    // Создаем файл id
                    ofstream id_file(user_dir + "/id");
                    id_file << uid << endl;
                    id_file.close();

                    // Создаем файл home
                    ofstream home_file(user_dir + "/home");
                    home_file << homedir << endl;
                    home_file.close();

                    // Создаем файл shell
                    ofstream shell_file(user_dir + "/shell");
                    shell_file << shell << endl;
                    shell_file.close();

                    cout << "[VFS] Created: " << username << " (UID: " << uid << ")" << endl;
                    count++;
                }
            }
            catch (...) {
                continue;
            }
        }
    }

    // 3. Если нет пользователей, создаем тестового
    if (count == 0) {
        string test_dir = mount_point + "/testuser";
        system(("mkdir -p " + test_dir).c_str());

        ofstream(test_dir + "/id") << "1000" << endl;
        ofstream(test_dir + "/home") << "/home/testuser" << endl;
        ofstream(test_dir + "/shell") << "/bin/bash" << endl;

        cout << "[VFS] Created test user" << endl;
    }

    cout << "[VFS] Total users created: " << (count > 0 ? count : 1) << endl;
    cout << "[VFS] Check with: ls " << mount_point << endl;
}

void stop_users_vfs() {
    cout << "[VFS] Stopped" << endl;
}