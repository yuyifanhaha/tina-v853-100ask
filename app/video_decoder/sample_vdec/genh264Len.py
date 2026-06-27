# -*- coding: utf-8 -*-

def find_start_code(data, start):
    for i in range(start, len(data) - 4):
        if data[i] == 0x00 and data[i+1] == 0x00 and data[i+2] == 0x01:
            return i, 3
        if data[i] == 0x00 and data[i+1] == 0x00 and data[i+2] == 0x00 and data[i+3] == 0x01:
            return i, 4
    return -1, 0

def extract_frame_lengths(input_file, output_file):
    with open(input_file, 'rb') as f:
        data = f.read()

    frame_lengths = []
    start = 0
    while True:
        start_code, offset = find_start_code(data, start)
        if start_code == -1:
            break
        next_start_code, _ = find_start_code(data, start_code + offset)
        if next_start_code == -1:
            frame_length = len(data) - start_code
        else:
            frame_length = next_start_code - start_code
        frame_lengths.append((frame_length, offset))
        start = start_code + frame_length

    with open(output_file, 'w', encoding='utf-8') as f:
        for length, offset in frame_lengths:
            separator = ' ' * (8 - len(str(length)))
            f.write("{}{}".format(length, separator))

    print(f"帧长度已写入文件 {output_file}")

# 使用示例
extract_frame_lengths('input.h264', 'frame_lengths.txt')

