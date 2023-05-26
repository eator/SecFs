# 生成包含800个字节的文件，每个字节都是字符'0'
filename = "Jerry"
byte_count = 800
byte_value = '0'

with open(filename, 'w') as file:
    file.write(byte_value * byte_count)

print("ffseek 测试文件生成完成")

