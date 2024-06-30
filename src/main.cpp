#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <vector>
#include <iostream>
#include <sys/stat.h>
#include <dirent.h>
#include "ConvertColorSpace.h"
#include "InputCmdParser.h"
#include "ThreadPool.h"
#include "Timer.h"

void ProcessImage(const std::string& fileName, const std::string& format, const std::string& pattern) {
    // 读取文件到内存
    struct stat stat_buf;
    int rc = stat(fileName.c_str(), &stat_buf);
    if (rc != 0) {
        std::cerr << "无法读取文件: " << fileName << std::endl;
        return;
    }
    uint dataSize = stat_buf.st_size;
    uchar* buffer = new uchar[dataSize];
    FILE* file = fopen(fileName.c_str(), "rb");
    fread(buffer, 1, dataSize, file);
    fclose(file);

    ConvertColorSpace converter(1936, 1112, dataSize);  // 假设宽度和高度为1920x1080
    cv::Mat img;

    if (format == "nv12") {
        img = converter.ConvertNV12ToJpeg(buffer);
    } else if (format == "bayer") {
        img = converter.ConvertBayerToJpeg(buffer, pattern);
    } else {
        std::cerr << "未知的格式: " << format << std::endl;
        delete[] buffer;
        return;
    }

    std::string outputFileName = fileName + ".jpg";
    converter.SaveImage(outputFileName, img);
    delete[] buffer;
}

void ProcessDirectory(const std::string& dirName, const std::string& format, const std::string& pattern, ThreadPool& pool) {
    DIR* dir = opendir(dirName.c_str());
    if (!dir) {
        std::cerr << "无法打开目录: " << dirName << std::endl;
        return;
    }
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) {
            std::string fileName = dirName + "/" + entry->d_name;
            pool.enqueue([fileName, format, pattern] {
                ProcessImage(fileName, format, pattern);
            });
        }
    }
    closedir(dir);
}

int main(int argc, const char** argv) {
    InputCmdParser input(argc, argv);

    if (input.CmdOptionExists("-i") && input.CmdOptionExists("--format")) {
        Timer timer;
        std::string inputFileName = input.GetCmdOption("-i");
        std::string format = input.GetCmdOption("--format");
        std::string pattern;
        if (format == "bayer" && input.CmdOptionExists("--pattern")) {
            pattern = input.GetCmdOption("--pattern");
        }

        ThreadPool pool(4);
        struct stat stat_buf;
        int rc = stat(inputFileName.c_str(), &stat_buf);
        if (rc == 0 && S_ISDIR(stat_buf.st_mode)) {
            ProcessDirectory(inputFileName, format, pattern, pool);
        } else {
            ProcessImage(inputFileName, format, pattern);
        }
    } else {
        std::cerr << "缺少必要参数" << std::endl;
        std::cerr << "使用方法: " << argv[0] << " -i <input> --format <format> [--pattern <pattern>]" << std::endl;
    }

    return 0;
}
