#!/bin/bash

# 检查参数数量
if [ $# -ne 1 ]; then
  echo "用法: $0 <日期时间>"
  echo "示例: $0 25-02-24-143000"
  exit 1
fi

# 获取车辆号
vehicle_id=$(basename "$HOME")
if [[ ! "$vehicle_id" =~ ^zpmc[0-9]+$ ]]; then
  echo "无法从 \$HOME 中获取车辆号。\$HOME=$HOME"
  exit 1
fi

# 解析故障时间A
datetime=$1
year=$(echo "$datetime" | cut -d'-' -f1)
month=$(echo "$datetime" | cut -d'-' -f2)
day=$(echo "$datetime" | cut -d'-' -f3)
time=$(echo "$datetime" | cut -d'-' -f4)
full_year="20$year"  # 假设年份为 20xx
A_timestamp="${full_year}${month}${day}-${time}"  # 例如 20250224-143000

# 日志目录
log_dir="/home/${vehicle_id}/.ros/log/udi_localization/"

# 在容器内查找并排序日志文件
files=$(docker exec localization-slam bash -c "
  ls $log_dir/robust_localization_node.${vehicle_id}.invalid-user.log.INFO.* 2>/dev/null | sort
")

# 转换为数组
IFS=$'\n' read -r -d '' -a file_array <<< "$files"

# 查找时间戳 <= A 的最后一个文件
last_file=""
for file in "${file_array[@]}"; do
  timestamp=$(echo "$file" | sed 's/.*INFO\.\(.*\)\..*/\1/')
  if [[ "$timestamp" <= "$A_timestamp" ]]; then
    last_file="$file"
  else
    break
  fi
done

# 检查是否找到文件
if [ -z "$last_file" ]; then
  echo "未找到符合条件的日志文件。"
  exit 1
fi

# 找到下一个文件（如果存在）
next_file=""
for file in "${file_array[@]}"; do
  if [[ "$file" > "$last_file" ]]; then
    next_file="$file"
    break
  fi
done

# 判断A是否在时间范围内
if [ -z "$next_file" ]; then
  file_path="$last_file"  # 没有下一个文件，A在最后一个文件范围内
else
  next_timestamp=$(echo "$next_file" | sed 's/.*INFO\.\(.*\)\..*/\1/')
  if [[ "$A_timestamp" < "$next_timestamp" ]]; then
    file_path="$last_file"  # A 在 last_file 的时间范围内
  else
    echo "未找到包含时间A的日志文件。"
    exit 1
  fi
fi

# 设置目标目录并复制文件
target_dir=~/Documents/xiongxueliang/
mkdir -p "$target_dir"
docker cp "localization-slam:$file_path" "$target_dir"

echo "日志文件已复制到 $target_dir"
