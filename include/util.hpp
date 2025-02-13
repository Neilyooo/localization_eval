//
// Created by alter on 28/8/2019.
//

#ifndef UNITY_LANS_GZ_UTIL_UTIL_HPP_
#define UNITY_LANS_GZ_UTIL_UTIL_HPP_

#include <pwd.h>
#include <dirent.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <glog/logging.h>
#include <iostream>
#include <array>
#include <memory>


#include "mkdir_p.hpp"

class TicToc {
 public:
  TicToc() {
    tic();
  }

  void tic() {
    start = std::chrono::system_clock::now();
  }

  double toc() {
    end = std::chrono::system_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    return elapsed_seconds.count();
  }

 private:
  std::chrono::time_point<std::chrono::system_clock> start, end;
};

inline double Norm(double x, double y, double z) {
  return sqrt(x * x + y * y + z * z);
}

template<typename PointT>
inline double PointRange(PointT p) {
  return Norm(p.x, p.y, p.z);
}

template<typename PointT>
inline double PointDistance(PointT p1, PointT p2) {
  return Norm(p1.x - p2.x, p1.y - p2.y, p1.z - p2.z);
}

template<typename PointT>
inline double PointDistance2d(PointT p1, PointT p2) {
  return Norm(p1.x - p2.x, p1.y - p2.y, 0);
}

template<typename PointT>
inline double Azimuth(PointT p) {
  return atan2(-p.y, p.x);
}

inline int GetPathUnderDir(const std::string& directory,
                           std::vector<std::string>& paths) {
  DIR* dp;
  struct dirent* dirp;
  if ((dp = opendir(directory.c_str())) == NULL) {
    std::cerr << "Error opening " << directory << std::endl;
    return errno;
  }
  while ((dirp = readdir(dp)) != NULL) {
    paths.push_back(directory + '/' + dirp->d_name);
  }
  closedir(dp);
  std::sort(paths.begin(), paths.end());
  paths.erase(paths.begin(), paths.begin() + 2);
  return 0;
}

inline std::string exec(const char* cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

inline int SetGoogleLogDir() {
  google::InstallFailureSignalHandler();
  google::InstallFailureWriter([](const char* data, int size){LOG(INFO).write(data,size);google::FlushLogFiles(google::INFO);});

  const char* homedir;
  if ((homedir = getenv("HOME")) == NULL) {
    homedir = getpwuid(getuid())->pw_dir;
  }
  std::string log_path = std::string(homedir) + "/.ros/log/udi_zpmc_monitor_log/";
  struct stat info;
  if (stat(log_path.c_str(), &info) != 0) {
    if (mkdir_p(log_path.c_str()) != 0){
      std::cout<<"fail to create logging directory"<<std::endl;
      return -1;
    }
  }
  FLAGS_log_dir = log_path;
  FLAGS_log_prefix = false;
  LOG(INFO) << exec("uptime -p");
  LOG(INFO) << exec("apt-cache policy udi-zpmc-lidar-pack");
  google::FlushLogFiles(google::INFO);
  FLAGS_log_prefix = true;
  return 0;
}

inline int SetCachePath(std::string* cache_path) {
  const char* homedir;
  if ((homedir = getenv("HOME")) == NULL) {
    homedir = getpwuid(getuid())->pw_dir;
  }
  *cache_path = std::string(homedir) + "/.config/udi_zpmc_monitor_log/";
  struct stat info;
  if (stat(cache_path->c_str(), &info) != 0) {
    if (mkdir_p(cache_path->c_str()) != 0)
      return -1;
  }
  std::string cmd = "touch "+*cache_path + "pose";
  system(cmd.c_str());
  cmd = "touch "+*cache_path + ".pose.swap";
  system(cmd.c_str());
  return 0;
}

#endif //UNITY_LANS_GZ_UTIL_UTIL_HPP_
