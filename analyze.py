import re
from datetime import datetime
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as mdates

MAX_SEQ = 65536

class LogInfo:
  '''
  日志数据三元组，分别为：
  `timestamp`: 时间戳
  `icmp_seq`: 对应的 ICMP 报文序列号
  `rtt_ms`: 这条日志的 RTT
  '''
  def __init__(self,
               timestamp: int | float | str,
               icmp_seq: int | float,
               rtt_ms: int | float) -> None:
    self.timestamp = datetime.fromtimestamp(float(timestamp))
    self.icmp_seq = int(icmp_seq)
    self.rtt_ms = int(rtt_ms)

def extract_data(file_path: str) -> list[LogInfo]:
  '''
  从文件路径 `file_path` 中读取日志文件，
  提取时间戳、ICMP 报文序列号及 RTT
  '''
  pattern = r'\[(.*?)\] .*icmp_seq=(\d+) .* time=(\d+) ms'
  log_entries = []
  with open(file_path, 'r') as file:
    for line in file:
      result = re.search(pattern, line)
      if result is not None:
        log_entries.append(LogInfo(*result.groups()))
  return log_entries

def count_loss_packets(packet_list: list[LogInfo]) -> tuple[int, tuple[int, int], tuple[int, int]]:
  '''
  从报文数据列表中提取一个信息三元组，元素解释分别为：
  1. 丢包数量；
  2. 最长的一个丢包序列，只给出升序闭区间首尾端点；
  3. 最长的一个未丢包序列，只给出升序闭区间首尾端点。
  '''
  global MAX_SEQ
  loss_cnt = 0
  loss_interval = [packet_list[0].icmp_seq, packet_list[0].icmp_seq]
  multi_consecutive_intervals = []

  consecutive_interval = loss_interval.copy()
  for idx, packet in enumerate(packet_list[1:], start=1):
    distance = (packet.icmp_seq - packet_list[idx - 1].icmp_seq) % MAX_SEQ
    if distance > 1: # 有缺省项
      loss_cnt += distance - 1
      # 因为缺省了，所以序号不再连续，要把连续序列推入列表中
      if consecutive_interval[0] != consecutive_interval[1]:
        multi_consecutive_intervals.append(consecutive_interval)
      consecutive_interval = [packet.icmp_seq, packet.icmp_seq]
      # 判断一下当前的缺省序列是否要更新，保证始终只取长度最大的缺省序列
      if distance > ((loss_interval[1] - loss_interval[0]) % MAX_SEQ):
        loss_interval = [packet_list[idx - 1].icmp_seq, packet.icmp_seq]
    else: # 如果序号值始终连续，只更改 `consecutive_interval` 的右端点
      consecutive_interval[1] = packet.icmp_seq

  # 处理最后一个序列
  if consecutive_interval[0] != consecutive_interval[1]:
    multi_consecutive_intervals.append(consecutive_interval)

  # 复用一下变量名，懒得新取
  consecutive_interval = max(multi_consecutive_intervals, key=lambda x: (x[1] - x[0]) % MAX_SEQ)
  loss_interval.sort() # 排个序更方便使用
  consecutive_interval.sort()
  return loss_cnt, loss_interval, consecutive_interval

def probability_calculator(packet_list: list[LogInfo], num_loss: int) -> tuple[float, float]:
  '''
  从报文数据列表中计算不同条件下发生的概率，返回的元组元素解释为：
  1. 收到 `request #N` 的情况下收到 `request #(N+1)` 的概率
  2. 没有收到 `request #N` 的情况下收到 `request #(N+1)` 的概率。
  '''
  global MAX_SEQ
  always_normal = 0 # 收到 `request #N` 的情况下收到 `request #(N+1)` 的次数
  normal_after_loss = 0 # 没有收到 `request #N` 的情况下收到 `request #(N+1)` 的次数
  for idx, packet in enumerate(packet_list[1:-1], start=1):
    if ((packet.icmp_seq - packet_list[idx - 1].icmp_seq) % MAX_SEQ) > 1:
      normal_after_loss += 1
    else: # 一个简单的条件概率问题，公式就不给出了
      always_normal += 1
  return (always_normal / len(packet_list) * 100.0, # 收到 #N 的情况下收到 #(N+1) 的概率
          normal_after_loss / num_loss * 100.0) # 没有收到 #N 的情况下收到 #(N+1) 的概率


if __name__ == '__main__':
  summarization = extract_data('./data.txt')
  num_loss, longest_loss, longest_consecutive = count_loss_packets(summarization)
  p_always_normal, p_normal_after_loss = probability_calculator(summarization, num_loss)

  # 报文分析摘要
  print(f'总报文数：{len(summarization) + num_loss}\n'
        f'成功接收：{len(summarization)}\n'
        f'丢包数量：{num_loss}\n'
        f'总交付率：{(1 - (num_loss / (len(summarization) + num_loss))) * 100.0:.3f} %\n'
         '\n'
        f'最小的 RTT 是：{min(summarization, key=lambda x: x.rtt_ms).rtt_ms} ms\n'
        f'最大的 RTT 是：{max(summarization, key=lambda x: x.rtt_ms).rtt_ms} ms\n'
        f'最长的报文丢失发生在：{longest_loss}，共丢失 {longest_loss[1] - longest_loss[0]} 个数据报\n'
        f'最长的报文接收发生在：{longest_consecutive}，共接收 {longest_consecutive[1] - longest_consecutive[0]} 个数据报\n'
        f'request #N 被响应时 request #(N+1) 也被响应的概率：{p_always_normal:.3f} %\n'
        f'request #N 丢包时能收到 request #(N+1) 的概率：{p_normal_after_loss:.3f} %\n')

  # 绘制 RTT 在时间上分布的直方图
  fig, ax = plt.subplots(figsize=(6, 8))
  ax.plot([x.timestamp for x in summarization],
          [x.rtt_ms for x in summarization],
          label='RTT (ms)')
  ax.xaxis.set_major_locator(mdates.AutoDateLocator())
  ax.xaxis.set_major_formatter(mdates.DateFormatter('%H:%M:%S'))
  plt.xticks(rotation=45)
  plt.title('The value of RTT at different times')
  plt.xlabel('times')
  plt.ylabel('RTT (ms)')
  plt.legend()
  plt.savefig('column_diagram.png')

  # 绘制 RTT 的频率分布图
  fig, ax = plt.subplots(figsize=(7, 8))
  unique_rtt, rtt_counts = np.unique([x.rtt_ms for x in summarization], return_counts=True)
  ax.bar(unique_rtt,
         (rtt_counts / len(summarization)) * 100.0,
         label='The proportion of RTT (%)')
  plt.title('The proportion of different RTT in total time')
  plt.xlabel('RTT (ms)')
  plt.ylabel('The proportion of RTT (%)')
  plt.legend()
  plt.savefig('frequency_map.png')

  # 绘制 RTT 在不同报文间的波动的散点图
  fig, ax = plt.subplots(figsize=(6, 8))
  ax.scatter([x.rtt_ms for x in summarization[:-1]],
             [x.rtt_ms for x in summarization[1:]],
             s=10,
             label='The next RTT (ms)')
  plt.title('The next RTT in different time')
  plt.xlabel('RTT (ms)')
  plt.ylabel('The next RTT (ms)')
  plt.legend()
  plt.savefig('next_rtt.png')
