# Monitor Log 
# Version: v1.0

## 简介

本软件包实现了一个用于 ROS 的 监控、故障管理和连续数据记录 节点。该节点监控 定位 和 传感器 话题，检测异常情况（故障），发布相应的故障代码，并在故障事件发生期间 持续 将相关数据记录到 rosbag 文件中。

## 概述

该节点 订阅 以下话题：

- `/fastlio/odometry` （`nav_msgs/Odometry`）：提供 实时定位 输出。
- `/mag_nail` （`nav_msgs/Odometry`）：提供 地面真值（参考）定位数据。
- `/veloydne_first/points_raw` （`sensor_msgs/PointCloud2`）：第一个 LiDAR 传感器数据。
- `/veloydne_second/points_raw` （`sensor_msgs/PointCloud2`）：第二个 LiDAR 传感器数据。
- `/imu_plc` （`sensor_msgs/Imu`）：IMU 传感器 数据。

当任何 故障条件 触发时，节点将启动 连续记录会话，具体行为如下：

- 写入 最近 120 秒 的 缓冲（预触发）数据 到新的 rosbag 文件中。
- 持续追加 新接收的数据到 同一 rosbag 文件。
- 当故障状态 清除（即在设定时间内未检测到新故障）后，停止记录。

## 故障代码及触发条件

### E001: 定位误差超标

触发条件：

- 计算 `/fastlio/odometry`（实时定位数据） 与 `/mag_nail`（地面真值）之间的 误差。
- 误差分解为 纵向误差 和 横向误差，依据 `/mag_nail` 提取的 航向角（yaw） 进行计算。
- 当以下任一条件满足时触发故障：
  - 纵向误差 > 0.8m
  - 横向误差 > 0.1m

### E002: first LiDAR 数据无效

触发条件：

- 监控 `/veloydne_first/points_raw` 话题。
- 若接收到的消息为空（即 `width` 或 `height` 为 `0`），则触发故障。

### E003: second LiDAR 数据无效

触发条件：

- 监控 `/veloydne_second/points_raw` 话题。
- 若接收到的消息为空（即 `width` 或 `height` 为 `0`），则触发故障。

### E004: IMU 数据无效

触发条件：

- 监控 `/imu_plc` 话题。
- 若收到的 IMU 数据中 `linear acceleration` 或 `angular velocity` 字段包含 NaN 值，则触发故障。

### E005: 传感器超时

触发条件：

- 监控所有传感器数据的接收时间。
- 若任何监控传感器（LiDAR 或 IMU）在 0.5 秒内未接收到数据，则触发故障。

## 连续记录行为

### 开始记录

1. 调用 `startContinuousRecording()`。
2. 创建新的 rosbag 文件，文件名格式如下：
   ```
   ErrorCode-diagnose.<用户名>.<主机名>.<YYYYMMDD-HHMMSS>.bag
   ```
3. 写入过去 120 秒的缓冲数据（包括 `/fastlio/odometry`、`/mag_nail` 和 `/imu_plc`）。
4. 标记记录状态，后续 新接收到的消息 将持续追加到 rosbag 文件中。

### 故障清除检查

1. 一个定时器（`faultClearCheck`）会周期性检查 是否检测到新的故障。
2. 若 5 秒内未检测到新的故障，则认为故障已清除，并调用 `stopContinuousRecording()` 关闭 rosbag 文件。
3. 这样可以确保 记录文件 包含完整的故障时段数据（包括故障发生前的缓冲数据和故障期间的实时数据）。

## 构建与运行说明

### 依赖

- ROS （Kinetic、Melodic 或更高版本）
- glog 和 gflags
- 支持 C++11 的编译器

### 构建

```bash
catkin_make
source devel/setup.bash
```

### 运行

#### 使用 `roslaunch` 启动

```bash
roslaunch monitor_log monitor_log.launch
```

## 订阅的话题

- `/fastlio/odometry` (`nav_msgs/Odometry`)
- `/mag_nail` (`nav_msgs/Odometry`)
- `/veloydne_first/points_raw` (`sensor_msgs/PointCloud2`)
- `/veloydne_second/points_raw` (`sensor_msgs/PointCloud2`)
- `/imu_plc` (`sensor_msgs/Imu`)

## 发布的话题

- `/fault_codes` (`std_msgs/String`)：故障代码消息（若启用）。

## 日志记录

- 该节点使用 ROS 日志 和 Google glog 记录事件和故障检测。
- 请确保 glog 在你的环境中 正确配置 以捕获日志输出。

## 自定义参数 （可选目前仅在代码里写死）

### 故障阈值

- 纵向误差阈值（0.8m） 和 横向误差阈值（0.1m） 可在 `MonitoringModule` 类中修改。

### 记录缓冲时长

- 预缓冲时间 默认 120 秒，可在 `DataRecorder` 中修改 `RECORD_WINDOW_` 参数。

### 故障清除超时

- 若 `faultClearCheck` 计时器超过 5 秒未检测到新故障，则认为故障已清除。
- 该值可在 `faultClearCheck` 计时器回调函数中调整。
  

