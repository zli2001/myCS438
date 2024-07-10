#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024 // 1KB buffer size

int main() {
    FILE *input_file, *output_file;
    char buffer[BUFFER_SIZE];
    size_t bytes_read;

    // 打开要读取的二进制文件
    input_file = fopen("../test/data.zip", "rb");
    if (input_file == NULL) {
        perror("Error opening input file");
        return 1;
    }

    // 创建要写入的二进制文件
    output_file = fopen("output", "wb");
    if (output_file == NULL) {
        perror("Error opening output file");
        fclose(input_file);
        return 1;
    }

    // 读取并写入文件内容
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), input_file)) > 0) {
        fwrite(buffer, 1, bytes_read, output_file);
    }

    // 关闭文件
    fclose(input_file);
    fclose(output_file);

    printf("File copied successfully.\n");

    return 0;
}
