
//
// 完整的监控／故障码／数据录制节点示例
//
#include <ros/ros.h>
#include <nav_msgs/Odometry.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/Imu.h>
#include <rosbag/bag.h>
#include <std_msgs/String.h>

#include <sstream>
#include <mutex>
#include <thread>
#include <deque>
#include <map>
#include <ctime>
#include <iomanip>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <cmath>
#include <glog/logging.h>
#include <gflags/gflags.h>
#include <functional>
#include <future>
#include "util.hpp"
// **********************************************************
// FaultManager：负责记录和发布故障码（例如 E001,E002...）
// **********************************************************
class FaultManager {
public:
  FaultManager(ros::NodeHandle &nh) {
    fault_pub_ = nh.advertise<std_msgs::String>("fault_codes", 10);
  }
  // 当检测到故障时调用（例如定位误差过大、传感器数据无效或超时）
  void reportFault(const std::string &code, const std::string &description) {
    std::lock_guard<std::mutex> lock(fault_mutex_);
    std_msgs::String msg;
    std::ostringstream oss;
    oss << "[" << code << "] " << description 
        << " at " << ros::Time::now().toSec();
    msg.data = oss.str();
    LOG(INFO)<<"["<<code << "] "<< description ;
    // fault_pub_.publish(msg);
    // ROS_WARN("%s", msg.data.c_str());
    // 这里“记录一次后清空”的逻辑由上层模块调用前确保状态重置
  }
private:
  ros::Publisher fault_pub_;
  std::mutex fault_mutex_;
};

// **********************************************************
// DataRecorder：负责订阅、缓存并在故障触发时录制 rosbag
// **********************************************************
class DataRecorder {
public:
  DataRecorder(ros::NodeHandle &nh) : nh_(nh) {
    // 订阅各个需要录制的话题
    odom_sub_ = nh_.subscribe("fastlio/odometry", 100, &DataRecorder::odomCallback, this);
    mag_sub_  = nh_.subscribe("mag_nail", 100, &DataRecorder::magCallback, this);
    // first_lidar_sub_ = nh_.subscribe("veloydne_first/points_raw", 100, &DataRecorder::first_lidarCallback, this);
    // second_lidar_sub_ = nh_.subscribe("veloydne_first/points_raw", 100, &DataRecorder::second_lidarCallback, this);
    imu_sub_ = nh_.subscribe("imu_plc", 100, &DataRecorder::imuCallback, this);

    // recording_thread_ = std::thread(&DataRecorder::recordingLoop, this);
  }
  ~DataRecorder() {
    // {
    //   std::lock_guard<std::mutex> lock(cv_mutex_);
    //   exit_flag_ = true;
    // }
    // cv_.notify_one();
    // if(recording_thread_.joinable())
    //   recording_thread_.join();
    stopContinuousRecording(); //保证线程正常退出停止录制防止泄露
  }
    //停止录制
  void stopContinuousRecording() {
    std::lock_guard<std::mutex> lock(continuous_mutex_);
    if(!continuous_recording_active_) return;
    try{
      current_bag_.close();
    } catch (const rosbag::BagIOException& e) {
      ROS_ERROR("Failed to close rosbag file: %s", e.what());
      return;
    }
    continuous_recording_active_ = false;
    ROS_WARN("Continuous recording stopped.");

  }

  void startContinuousRecording(const std::string &code) {
    std::lock_guard<std::mutex> lock(continuous_mutex_);
    if(continuous_recording_active_) return;

    //record bag
    std::time_t  t = std::time(nullptr);
    std::tm *now_tm = std::localtime(&t);
    std::ostringstream time_suffix;
    time_suffix << std::put_time(now_tm, "%Y%m%d-%H%M%S");
    const char *username = getpwuid(getuid())->pw_name;
    char hostname[20];
    gethostname(hostname, sizeof(hostname));
    const char* homedir;
    if ((homedir = getenv("HOME")) == NULL) {
      homedir = getpwuid(getuid())->pw_dir;
    }
    // 这里可以通过 ROS 参数指定录制目录，这里暂时写死为 /tmp/
    std::string log_dir = std::string(homedir) + "/.ros/log/unity-lans-gz/";;
    std::string bag_filename = log_dir + code + "-diagnose." + std::string(username) + "." +
                               std::string(hostname) + "." + time_suffix.str() + ".bag";
            
    try{
      current_bag_.open(bag_filename, rosbag::bagmode::Write);

    } catch (const rosbag::BagIOException& e) {
      ROS_ERROR("Failed to open rosbag file: %s", e.what());
      return;
    }

    //将数据队里写入bag
    {//fastlio/odom
        std::lock_guard<std::mutex> lock(odom_mutex_);
        for(const auto &item : odom_queue_) {
          current_bag_.write("fastlio/odometry", item.first, item.second);
        }
    }
    //mag_nail
    {
        std::lock_guard<std::mutex> lock(mag_mutex_);
        for(const auto &item : mag_queue_) {
          current_bag_.write("mag_nail", item.first, item.second);
        }
    }

    //imu_plc
    {
        std::lock_guard<std::mutex> lock(imu_mutex_);
        for(const auto &item : imu_queue_) {
          current_bag_.write("imu_plc", item.first, item.second);
        }
    }
    //将数据写入bag
    continuous_recording_active_ = true;
    ROS_WARN("Continuous recording started.");
  }
  // 外部调用此接口触发录制（例如监控模块检测到故障时）
  /**
   * @brief 已弃用，需要使用 startContinuousRecording
   * 
   */
  void triggerRecording() {
    std::lock_guard<std::mutex> lock(cv_mutex_);
    trigger_ = true;
    cv_.notify_one();
  }
private:
  ros::NodeHandle nh_;
  ros::Subscriber odom_sub_, mag_sub_, first_lidar_sub_, imu_sub_, second_lidar_sub_;

  // 使用 deque 存储一段时间内的消息（key 为消息类型）
  std::deque<std::pair<ros::Time, nav_msgs::Odometry>> odom_queue_;
  std::deque<std::pair<ros::Time, nav_msgs::Odometry>> mag_queue_;
  std::deque<std::pair<ros::Time, sensor_msgs::PointCloud2>> first_lidar_queue_;
  std::deque<std::pair<ros::Time, sensor_msgs::PointCloud2>> second_lidar_queue_;
  std::deque<std::pair<ros::Time, sensor_msgs::Imu>> imu_queue_;

  // 各队列的保护互斥量
  std::mutex odom_mutex_, mag_mutex_, lidar_mutex_, imu_mutex_;
  std::mutex continuous_mutex_;
  bool continuous_recording_active_ =false;
  rosbag::Bag current_bag_;
  const double RECORD_WINDOW_ = 120.0; // 记录最近45秒的数据

  // 用于触发录制的条件变量和标志
  std::mutex cv_mutex_;
  std::condition_variable cv_;
  bool trigger_ = false;
  bool exit_flag_ = false;
  std::thread recording_thread_;

  // 模板函数：将消息推入对应队列，同时删除超出时间窗的数据
  template<typename T>
  void pushMessage(std::deque<std::pair<ros::Time, T>> &queue,
                   std::mutex &mtx, const ros::Time &stamp, const T &msg) {
    std::lock_guard<std::mutex> lock(mtx);
    queue.push_back(std::make_pair(stamp, msg));
    // 删除比当前时间早超过 RECORD_WINDOW_ 的消息
    while (!queue.empty() && (stamp - queue.front().first).toSec() > RECORD_WINDOW_) {
      queue.pop_front();
    }
  }

  // 回调函数：订阅 fastlio/odom 将数据推入队列，同时在连续录制时连续写入数据bag
  void odomCallback(const nav_msgs::Odometry::ConstPtr &msg) {
    pushMessage(odom_queue_, odom_mutex_, msg->header.stamp, *msg);
    std::lock_guard<std::mutex> lock(continuous_mutex_);
    if(continuous_recording_active_) {
      current_bag_.write("fastlio/odometry", msg->header.stamp, *msg);
    }
  }
  // 回调函数：订阅 mag_nail（真值）将数据推入队列，同时在连续录制时连续写入数据bag
  void magCallback(const nav_msgs::Odometry::ConstPtr &msg) {
    pushMessage(mag_queue_, mag_mutex_, msg->header.stamp, *msg);
    std::lock_guard<std::mutex> lock(continuous_mutex_);
    if(continuous_recording_active_) {
      current_bag_.write("mag_nail", msg->header.stamp, *msg);
    }
  }
  // 回调函数：订阅 first lidar 数据
  /**
   * @brief 暂时弃用，发散情况频繁，雷达数据量较大，暂不考虑
   * 
   * @param msg 
   */
//   void firstlidarCallback(const sensor_msgs::PointCloud2::ConstPtr &msg) {
//     pushMessage(lidar_queue_, lidar_mutex_, msg->header.stamp, *msg);
//   }
//   回调函数：订阅 sec lidar 数据
//   void secondlidarCallback(const sensor_msgs::PointCloud2::ConstPtr &msg) {
//     pushMessage(lidar_queue_, lidar_mutex_, msg->header.stamp, *msg);
//   }
  // 回调函数：订阅 IMU 数据 将数据推入队列，同时在连续录制时连续写入数据bag
  void imuCallback(const sensor_msgs::Imu::ConstPtr &msg) {
    pushMessage(imu_queue_, imu_mutex_, msg->header.stamp, *msg);
    std::lock_guard<std::mutex> lock(continuous_mutex_);
    if(continuous_recording_active_) {
      current_bag_.write("imu_plc", msg->header.stamp, *msg);
    }
  }

  // 后台线程：等待触发信号后，将各个队列写入 rosbag 文件
//   void recordingLoop() {
//     while (ros::ok() && !exit_flag_) {
//       std::unique_lock<std::mutex> lock(cv_mutex_);
//       cv_.wait(lock, [this]() { return trigger_ || exit_flag_; });
//       if (exit_flag_) break;
//       // 复制当前各队列内容到局部变量（以减少阻塞）
//       decltype(odom_queue_) odom_copy;
//       decltype(mag_queue_) mag_copy;
//     //   decltype(lidar_queue_) lidar_copy;
//       decltype(imu_queue_) imu_copy;
//       {
//         std::lock_guard<std::mutex> lock1(odom_mutex_);
//         odom_copy = odom_queue_;
//       }
//       {
//         std::lock_guard<std::mutex> lock2(mag_mutex_);
//         mag_copy = mag_queue_;
//       }
//     //   {
//     //     std::lock_guard<std::mutex> lock3(lidar_mutex_);
//     //     lidar_copy = lidar_queue_;
//     //   }
//       {
//         std::lock_guard<std::mutex> lock4(imu_mutex_);
//         imu_copy = imu_queue_;
//       }
//       trigger_ = false;  // 重置触发状态
//       lock.unlock();

//       // 生成文件名：diagnose.<username>.<hostname>.<YYYYMMDD-HHMMSS>.bag
//       std::time_t t = std::time(nullptr);
//       std::tm *now_tm = std::localtime(&t);
//       std::ostringstream time_suffix;
//       time_suffix << std::put_time(now_tm, "%Y%m%d-%H%M%S");
//       const char *username = getpwuid(getuid())->pw_name;
//       char hostname[20];
//       gethostname(hostname, sizeof(hostname));
//       const char* homedir;
//       if ((homedir = getenv("HOME")) == NULL) {
//         homedir = getpwuid(getuid())->pw_dir;
//     }
//       // 这里可以通过 ROS 参数指定录制目录，这里暂时写死为 /tmp/
//       std::string log_dir = std::string(homedir) + "/.ros/log/unity-lans-gz/";;
//       std::string bag_filename = log_dir + "diagnose." + std::string(username) + "." +
//                                  std::string(hostname) + "." + time_suffix.str() + ".bag";

//       try {
//         rosbag::Bag bag;
//         bag.open(bag_filename, rosbag::bagmode::Write);
//         // 将各队列写入 rosbag
//         for (const auto &item : odom_copy)
//           bag.write("fastlio/odom", item.first, item.second);
//         for (const auto &item : mag_copy)
//           bag.write("mag_nail", item.first, item.second);
//         // for (const auto &item : lidar_copy)
//         //   bag.write("veloydne_first/points_raw", item.first, item.second);
//         for (const auto &item : imu_copy)
//           bag.write("imu_plc", item.first, item.second);
//         bag.close();
//         ROS_WARN("Recorded bag file: %s", bag_filename.c_str());
//       } catch (rosbag::BagException &e) {
//         ROS_ERROR("Error writing bag file: %s", e.what());
//       }
//     }
//     ROS_INFO("DataRecorder recording thread exiting.");
//   }
};

// **********************************************************
// MonitoringModule：订阅定位与传感器数据，检测误差或异常情况
// **********************************************************
class MonitoringModule {
public:
  // 构造函数传入 FaultManager 和 DataRecorder 的指针（外部统一管理）
  MonitoringModule(ros::NodeHandle &nh,
                   FaultManager *fault_manager,
                   DataRecorder *data_recorder)
      : nh_(nh), fault_manager_(fault_manager), data_recorder_(data_recorder) {
    // 定位相关订阅
    odom_sub_ = nh_.subscribe("fastlio/odometry", 100, &MonitoringModule::odomCallback, this);
    mag_sub_ = nh_.subscribe("mag_nail", 100, &MonitoringModule::magCallback, this);
    // 传感器订阅（用于数据有效性检查）
    first_lidar_sub_ = nh_.subscribe("veloydne_first/points_raw", 100, &MonitoringModule::firstlidarCallback, this);
    second_lidar_sub_ = nh_.subscribe("veloydne_second/points_raw", 100, &MonitoringModule::secondlidarCallback, this);
    imu_sub_ = nh_.subscribe("imu_plc", 100, &MonitoringModule::imuCallback, this);
    // 定时器：检查各传感器是否超时（500ms 未更新）
    sensor_timeout_timer_ = nh_.createTimer(ros::Duration(0.2),
                                              &MonitoringModule::sensorTimeoutCheck, this);
    fault_clear_timer_ = nh_.createTimer(ros::Duration(0.5),
                                           &MonitoringModule::faultClearCheck, this);
    last_fault_time_ = ros::Time(0);
    // 初始化各传感器最后接收时间（以话题名为 key）
    /**
     * @brief  用于检测传感器是否超时
     * 
     */
    last_received_["veloydne_first/points_raw"] = ros::Time(0);
    last_received_["veloydne_second/points_raw"] = ros::Time(0);
    last_received_["imu_plc"] = ros::Time(0);
  }
private:
  ros::NodeHandle nh_;
  FaultManager *fault_manager_;
  DataRecorder *data_recorder_;

  ros::Subscriber odom_sub_, mag_sub_;
  ros::Subscriber first_lidar_sub_, imu_sub_, second_lidar_sub_;
  ros::Timer sensor_timeout_timer_;
  ros::Timer fault_clear_timer_;
  // 用于定位误差计算：保存最新接收到的 fastlio/odom 和 mag_nail
  std::mutex mutex_;
  ros::Time last_fault_time_;
  nav_msgs::Odometry latest_odom_;
  nav_msgs::Odometry latest_mag_;
  bool received_odom_ = false;
  bool received_mag_ = false;

  // 保存各传感器最后接收时间
  std::map<std::string, ros::Time> last_received_;

  // 设定的横向和纵向误差阈值（单位：米），可根据需要调整或通过参数配置
  const double THRESHOLD_LONGITUDINAL_ = 0.8;
  const double THRESHOLD_LATERAL_ = 0.1;

  std::mutex fault_mutex_;

  // 每次检测到故障时更新 last_fault_time_ 并启动录制
  void updateFaultTime(const std::string &code){
    std::lock_guard<std::mutex> lock(fault_mutex_);
    last_fault_time_ = ros::Time::now();
    data_recorder_->startContinuousRecording(code);

  }

  //定时器。如果超过设定时间没有故障，停止录制
  void faultClearCheck(const ros::TimerEvent &) {
    std::lock_guard<std::mutex> lock(fault_mutex_);
    if ((ros::Time::now() - last_fault_time_).toSec() > 5.0) {
      data_recorder_->stopContinuousRecording();
    }
  }
  // fastlio/odom 回调
  void odomCallback(const nav_msgs::Odometry::ConstPtr &msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    latest_odom_ = *msg;
    received_odom_ = true;
    if (received_mag_) computeLocalizationError();
  }
  // mag_nail 回调
  void magCallback(const nav_msgs::Odometry::ConstPtr &msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    latest_mag_ = *msg;
    received_mag_ = true;
    if (received_odom_) computeLocalizationError();
  }
  // 计算定位误差（横向和纵向误差）
  void computeLocalizationError() {
    // 差值（单位：米）
    double dx = latest_odom_.pose.pose.position.x - latest_mag_.pose.pose.position.x;
    double dy = latest_odom_.pose.pose.position.y - latest_mag_.pose.pose.position.y;
    // 使用真值（mag_nail）的朝向作为参考——计算 yaw
    double qx = latest_mag_.pose.pose.orientation.x;
    double qy = latest_mag_.pose.pose.orientation.y;
    double qz = latest_mag_.pose.pose.orientation.z;
    double qw = latest_mag_.pose.pose.orientation.w;
    double yaw = std::atan2(2 * (qw * qz + qx * qy), 1 - 2 * (qy * qy + qz * qz));
    // 将误差矢量转换到真值坐标系中，得到纵向和横向分量
    double longitudinal_error = dx * std::cos(yaw) + dy * std::sin(yaw);
    double lateral_error = -dx * std::sin(yaw) + dy * std::cos(yaw);
    // std::cerr<<"longitudinal_error: "<<longitudinal_error<<std::endl;
    // std::cerr<<"lateral_error: "<<lateral_error<<std::endl;
    // ROS_INFO("timestamp: %f, longitudinal_error: %f, lateral_error: %f", latest_odom_.header.stamp.toSec(),longitudinal_error, lateral_error);
    // 如果任一误差超过阈值，则触发故障和录制
    if (std::abs(longitudinal_error) > THRESHOLD_LONGITUDINAL_ ||
        std::abs(lateral_error) > THRESHOLD_LATERAL_) {
      std::ostringstream oss;
      oss << "Localization error exceeded: longitudinal = " << longitudinal_error
          << ", lateral = " << lateral_error;
      fault_manager_->reportFault("E001", oss.str());
    //   data_recorder_->triggerRecording();
      updateFaultTime("E001");
    }
  }
  // lidar 回调：更新接收时间，并检查是否为空数据（此处简单判断 width==0）
  void firstlidarCallback(const sensor_msgs::PointCloud2::ConstPtr &msg) {
    last_received_["veloydne_first/points_raw"] = msg->header.stamp;
    if (msg->width == 0 || msg->height == 0) {
      fault_manager_->reportFault("E002", "Invalid First LiDAR data: empty point cloud");
    //   data_recorder_->triggerRecording();
      //updateFaultTime();
    }
    // 这里还可以进一步检查 msg.data 中是否存在 NaN
    
  }

    void secondlidarCallback(const sensor_msgs::PointCloud2::ConstPtr &msg) {
    last_received_["veloydne_second/points_raw"] = msg->header.stamp;
    if (msg->width == 0 || msg->height == 0) {
      fault_manager_->reportFault("E003", "Invalid Second LiDAR data: empty point cloud");
    //   data_recorder_->triggerRecording();
      //updateFaultTime();
    }
    // 这里还可以进一步检查 msg.data 中是否存在 NaN
    
  }
  // IMU 回调：更新接收时间，并检查加速度或角速度是否为 NaN
  void imuCallback(const sensor_msgs::Imu::ConstPtr &msg) {
    last_received_["imu_plc"] = msg->header.stamp;
    if (std::isnan(msg->linear_acceleration.x) ||
        std::isnan(msg->angular_velocity.x)) {
      fault_manager_->reportFault("E004", "Invalid IMU data: NaN values detected");
    //   data_recorder_->triggerRecording();
      updateFaultTime("E004");
    }
  }
  // 定时器回调：检查各传感器是否超过 500ms 未更新
  void sensorTimeoutCheck(const ros::TimerEvent &) {
    ros::Time now = ros::Time::now();
    for (const auto &item : last_received_) {
      if ((now - item.second).toSec() > 0.5) {
        std::ostringstream oss;
        oss << "Sensor timeout: no data from " << item.first << " for "
            << (now - item.second).toSec() << " seconds";
        fault_manager_->reportFault("E005", oss.str());
        // data_recorder_->triggerRecording();
        updateFaultTime("E005");
      }
    }
  }
};

// **********************************************************
// main()
// **********************************************************
int main(int argc, char **argv) {
  ros::init(argc, argv, "monitor_log");
  ros::NodeHandle nh;
  google::InitGoogleLogging(argv[0]);
  if(SetGoogleLogDir() != 0)
        {
            return 1;
        }

        google::ParseCommandLineFlags(&argc, &argv, true);
  // 实例化三个模块，注意各模块间通过指针相互调用
  FaultManager fault_manager(nh);
  DataRecorder data_recorder(nh);
  MonitoringModule monitor(nh, &fault_manager, &data_recorder);

  // 使用多线程 spinner，保证回调和后台线程正常运行
  ros::AsyncSpinner spinner(4);
  spinner.start();
  ros::waitForShutdown();
  return 0;
}
