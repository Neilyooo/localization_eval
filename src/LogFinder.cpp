#include "LogFinder.h"
#include <sstream>
#include <iostream>
#include <cstdlib>

LogFinder::LogFinder(const std::string& vehicle_id) : vehicle_id(vehicle_id) {
    log_dir = "/home/" + vehicle_id + "/.ros/log/udi_localization/";
}

std::string LogFinder::getATimestamp(const std::string& datetime) {
    std::string year = "20" + datetime.substr(0, 2);
    std::string month = datetime.substr(3, 2);
    std::string day = datetime.substr(6, 2);
    std::string time = datetime.substr(9, 6);
    return year + month + day + "-" + time;
}

std::vector<std::string> LogFinder::getLogFiles() {
    std::vector<std::string> files;
    std::string command = "docker exec localization-slam bash -c \"ls " + log_dir + 
                          "robust_localization_node." + vehicle_id + 
                          ".invalid-user.log.INFO.* 2>/dev/null | sort\"";

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Failed to run docker command." << std::endl;
        return files;
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        files.emplace_back(buffer);
    }
    pclose(pipe);
    return files;
}

std::string LogFinder::findLog(const std::string& datetime) {
    std::string A_timestamp = getATimestamp(datetime);
    std::vector<std::string> files = getLogFiles();

    std::string last_file;
    for (const auto& file : files) {
        if (file.find(A_timestamp) != std::string::npos) {
            last_file = file;
        }
    }

    if (last_file.empty()) {
        return "未找到符合条件的日志文件。";
    }

    std::string target_dir = "/home/" + vehicle_id + "/Documents/xiongxueliang/";
    copyLogFile(last_file, target_dir);
    return "日志文件已复制到 " + target_dir;
}

void LogFinder::copyLogFile(const std::string& file_path, const std::string& target_dir) {
    std::string command = "mkdir -p " + target_dir + " && docker cp localization-slam:" + file_path + " " + target_dir;
    system(command.c_str());
}
