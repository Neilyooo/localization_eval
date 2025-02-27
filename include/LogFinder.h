#ifndef LOGFINDER_H
#define LOGFINDER_H

#include <string>
#include <vector>

class LogFinder {
public:
    explicit LogFinder(const std::string& vehicle_id);

    // 查找日志文件
    std::string findLog(const std::string& datetime);

private:
    std::string vehicle_id;
    std::string log_dir;
    std::string getATimestamp(const std::string& datetime);
    std::vector<std::string> getLogFiles();
    void copyLogFile(const std::string& file_path, const std::string& target_dir);
};

#endif // LOGFINDER_H
