#include <opencv2/imgcodecs.hpp>
#include <opencv2/opencv.hpp>

#include <sys/stat.h>
#include <dirent.h>
#include <cassert>

#include <string>
#include <algorithm>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <iostream>

struct ImageInfo {
    std::string format;
    uint width;
    uint height;
};

class Timer {
public:
    Timer() : m_start(std::chrono::high_resolution_clock::now()) {}
    ~Timer() {
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> duration = end - m_start;
        std::cout << "耗时 " << duration.count() << " 秒" << std::endl;
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_start;
};

class ThreadPool {
public:
    ThreadPool(size_t threads = std::thread::hardware_concurrency()) : stop(false)
    {
        for(size_t i = 0; i < threads; ++i)
            workers.emplace_back(
                [this] {
                    for(;;) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock(this->queue_mutex);
                            this->condition.wait(lock,
                                                 [this]{ return this->stop || !this->tasks.empty(); });
                            if(this->stop && this->tasks.empty())
                                return;
                            task = std::move(this->tasks.front());
                            this->tasks.pop();
                        }
                        task();
                    }
                }
            );
    }

    template<class F>
    void enqueue(F f)
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::function<void()>(f));
        }
        condition.notify_one();
    }

    ~ThreadPool()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for(std::thread &worker: workers)
        worker.join();
    }

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

class InputCmdParser {
public:
    InputCmdParser(int& argc, const char** argv)
    {
        for (int i = 1; i < argc; ++i) {
            m_tokens.emplace_back(argv[i]);
        }
    }

    const std::string GetCmdOption(const std::string& option) const
    {
        auto it = std::find(m_tokens.begin(), m_tokens.end(), option);
        if (it != m_tokens.end() && ++it != m_tokens.end()) {
            return *it;
        }
        return "";
    }

    bool CmdOptionExists(const std::string& option) const
    {
        return std::find(m_tokens.begin(), m_tokens.end(), option) != m_tokens.end();
    }

private:
    std::vector<std::string> m_tokens;
};

class ConvertColorSpace {
public:
    ConvertColorSpace(uint width, uint height, uint dataSize);
    ~ConvertColorSpace();

    cv::Mat ConvertNV12ToJpeg(const void* pData);
    cv::Mat ConvertBayerToJpeg(const void* pData, const std::string& pattern);
    void SaveImage(const std::string& fileName, const cv::Mat& img);

private:
    uint   m_width;
    uint   m_height;
    uint   m_dataSize;
    uchar* m_data;
};

ConvertColorSpace::ConvertColorSpace(uint width, uint height, uint dataSize)
: m_width(width), m_height(height), m_dataSize(dataSize), m_data(nullptr)
{
    assert(m_dataSize > 0);
    m_data = new uchar[m_dataSize];
}

ConvertColorSpace::~ConvertColorSpace()
{
    if (m_data != nullptr)
    {
        delete[] m_data;
        m_data = nullptr;
    }
}

cv::Mat ConvertColorSpace::ConvertNV12ToJpeg(const void* pData)
{
    if (pData == nullptr)
    {
        throw std::invalid_argument("pData not be null");
    }
    cv::Mat yuvImg(m_height + m_height / 2, m_width, CV_8UC1, const_cast<void*>(pData));
    cv::Mat bgrImg;
    cv::cvtColor(yuvImg, bgrImg, cv::COLOR_YUV2BGR_NV12);

    if (bgrImg.empty())
    {
        throw std::runtime_error("Failed to convert NV12 to BGR");
    }
    return bgrImg;
}

cv::Mat ConvertColorSpace::ConvertBayerToJpeg(const void* pData, const std::string& pattern)
{
    cv::Mat bayerImg10bit(m_height, m_width, CV_16UC1, (void*)pData);
    cv::Mat bayerImg8bit(m_height, m_width, CV_8UC1);
    cv::Mat bgrImg(m_height, m_width, CV_8UC3);

    cv::convertScaleAbs(bayerImg10bit, bayerImg8bit, (float)(255/1023.0f));

    if (pattern == "rggb") {
        cv::cvtColor(bayerImg8bit, bgrImg, cv::COLOR_BayerBG2BGR);
    } else if (pattern == "grbg") {
        cv::cvtColor(bayerImg8bit, bgrImg, cv::COLOR_BayerGB2BGR);
    } else if (pattern == "gbrg") {
        cv::cvtColor(bayerImg8bit, bgrImg, cv::COLOR_BayerGR2BGR);
    } else if (pattern == "bggr") {
        cv::cvtColor(bayerImg8bit, bgrImg, cv::COLOR_BayerRG2BGR);
    }
    return bgrImg;
}

void ConvertColorSpace::SaveImage(const std::string& fileName, const cv::Mat& img)
{
    cv::imwrite(fileName, img);
}

void ProcessImage(const std::string &fileName, const ImageInfo& imgInfo, const std::string &pattern)
{
    struct stat stat_buf;
    int rc = stat(fileName.c_str(), &stat_buf);
    if (rc != 0)
    {
        std::cerr << "Unable to read the file: " << fileName << std::endl;
        return;
    }
    uint dataSize = stat_buf.st_size;
    uchar* buffer = new uchar[dataSize];
    FILE* file = fopen(fileName.c_str(), "rb");
    if (!file)
    {
        std::cerr << "Failed to open file:" << fileName << std::endl;
        delete[] buffer;
    }
    fread(buffer, 1, dataSize, file);
    fclose(file);

    ConvertColorSpace converter(imgInfo.width, imgInfo.height, dataSize);

    cv::Mat img;
    if (imgInfo.format == "nv12")
    {
        img = converter.ConvertNV12ToJpeg(buffer);
    }
    else if (imgInfo.format == "bayer")
    {
        img = converter.ConvertBayerToJpeg(buffer, pattern);
    }
    else
{
        std::cerr << "Unknown format: " << imgInfo.format << std::endl;
        delete[] buffer;
        return;
    }

    std::string outputFileName = fileName.substr(0, fileName.rfind(".")) + ".jpg";
    converter.SaveImage(outputFileName, img);
    delete[] buffer;
}

std::vector<std::string> ProcessDirectory(const std::string& dirName, const std::string& format)
{
    std::vector<std::string> files;
    DIR *dir = opendir(dirName.c_str());
    if (!dir)
    {
        std::cerr << "Unable to open the dir: " << dirName << std::endl;
        return files;
    }

    auto isFileOfType = [](const std::string& fileName, const std::string& type)->bool {
        std::string suffix = fileName.substr(fileName.find_last_of(".") + 1);
        std::transform(suffix.cbegin(), suffix.cend(), suffix.begin(), [](char c){return std::tolower(c);});
        if (suffix.find(type) != std::string::npos)
        {
            return true;
        }

        return false;
    };

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (entry->d_type == DT_REG)
        {
            if ("nv12" == format && isFileOfType(entry->d_name, "nv12"))
            {
                std::string fileName = dirName + "/" + entry->d_name;
                files.push_back(fileName);
            }
            else if ("bayer" == format && isFileOfType(entry->d_name, "raw"))
            {
                std::string fileName = dirName + "/" + entry->d_name;
                files.push_back(fileName);
            }
        }
    }
    closedir(dir);
    return files;
}

int main(int argc, const char **argv)
{
    InputCmdParser input(argc, argv);

    if (input.CmdOptionExists("-i") &&
        input.CmdOptionExists("--format") &&
        input.CmdOptionExists("--width") &&
        input.CmdOptionExists("--height"))
    {
        Timer timer;
        ImageInfo imgInfo;
        std::string inputFileName = input.GetCmdOption("-i");
        imgInfo.format = input.GetCmdOption("--format");
        imgInfo.width = stoi(input.GetCmdOption("--width"));
        imgInfo.height = stoi(input.GetCmdOption("--height"));
        std::string pattern;
        if (imgInfo.format == "bayer" && input.CmdOptionExists("--pattern"))
        {
            pattern = input.GetCmdOption("--pattern");
        }
        else
    {
            throw std::invalid_argument("bayer format need --pattern");
            return EXIT_FAILURE;
        }

        printf("Use default number of threads: %d\n", std::thread::hardware_concurrency());
        ThreadPool pool;

        struct stat stat_buf;
        int rc = stat(inputFileName.c_str(), &stat_buf);
        if (rc == 0 && S_ISDIR(stat_buf.st_mode))
        {
            auto files = ProcessDirectory(inputFileName, imgInfo.format);
            for (const auto& file: files)
            {
                pool.enqueue([file, imgInfo, pattern]{ProcessImage(file, imgInfo, pattern);});
            }
        }
        else
    {
            ProcessImage(inputFileName, imgInfo, pattern);
        }
    }
    else
{
        std::cerr << "Required parameters" << std::endl;
        std::cerr << "Usage: " << argv[0] << " -i <input> --format <format> --width <width> --height <height> [--pattern <pattern>]" << std::endl;
    }

    return 0;
}
