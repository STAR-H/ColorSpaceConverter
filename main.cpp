#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>
#include <vector>
#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>

struct ImageData {
    void* pData;
    std::string fileName;
};


class Timer {
public:
    Timer()
    {
        m_start = std::chrono::high_resolution_clock::now();
    }

    ~Timer()
    {
        m_end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> duration = m_end - m_start;
        printf("consume %f s\n", duration.count());
    }
private:
    std::chrono::high_resolution_clock::time_point m_start;
    std::chrono::high_resolution_clock::time_point m_end;

};

class InputCmdParser {
public:
    InputCmdParser(int &argc, const char** argv)
    {
        for (int i = 1; i < argc; ++i)
        {
            m_tokens.emplace_back(argv[i]);
        }
    }

    const std::string GetCmdOption(const std::string option) const
    {
        std::vector<std::string>::const_iterator citer;
        citer = std::find(m_tokens.begin(), m_tokens.end(), option);
        if (citer != m_tokens.end() && ++citer != m_tokens.end())
        {
            return *citer;
        }
        static const std::string empty("");

        return empty;
    }

    bool CmdOptionExists(const std::string& option) const
    {
        return std::find(m_tokens.begin(), m_tokens.end(), option) != m_tokens.end();
    }

    InputCmdParser(const InputCmdParser&) = delete;
    InputCmdParser& operator= (const InputCmdParser&) = delete;
private:
    std::vector<std::string> m_tokens;
};

class ConvertColorSpace {
public:
    ConvertColorSpace(uint width, uint height, uint dataSize):m_width(width),m_height(height),m_dataSize(dataSize)
    {
        m_data = NULL;
        assert(m_dataSize > 0);
#ifdef DEBUG
        printf("m_dataSize %d\n", m_dataSize);
#endif
        m_data = new uchar[m_dataSize];
    }

    ~ConvertColorSpace()
    {
        if (m_data != NULL)
        {
            delete [] m_data;
        }
    }

    const ImageData* GetImageData(const std::string fileName) const
    {
        assert(m_dataSize > 0);

        FILE* pFile = NULL;

        if (!fileName.empty())
        {
            pFile = fopen(fileName.c_str(), "rb+");

            if (pFile == NULL)
            {
                printf("Open %s failed!\n", fileName.c_str());
                return NULL;
            }
        }
        else
        {
            printf("%s fileName is NULL\n", __func__);
            return NULL;
        }


        uint size = fread(m_data, 1, m_dataSize, pFile);
#ifdef DEBUG
        printf("read from %s %d Byte\n", fileName.c_str(), size);
#endif

        fclose(pFile);

        ImageData* img = new ImageData;
        img->pData    = m_data;
        img->fileName = fileName;

        return img;
    }

    void SaveImage(const std::string& imageName, const cv::Mat& data) const
    {
        if (!imageName.empty() && !data.empty())
        {
            std::string saveName = imageName.substr(0, imageName.rfind("."));
            saveName += ".jpg";
            cv::imwrite(saveName, data);
#ifdef DEBUG
            printf("\ninput Image Name:\n %s\nsaveName:\n %s\n\n", imageName.c_str(), saveName.c_str());
#endif
        }
        else
        {
            printf("imageName(%d) or data(%d) is NULL\n", imageName.empty(), data.empty());
        }
    }

    ConvertColorSpace(const ConvertColorSpace&) = delete;
    ConvertColorSpace& operator= (const ConvertColorSpace&) = delete;

protected:
    uint m_width;
    uint m_height;
    uchar* m_data;
    uint m_dataSize;
};

class ConvertNV12ToJpeg : public ConvertColorSpace
{
public:
    ConvertNV12ToJpeg(uint width, uint height, uint dataSize) : ConvertColorSpace(width, height, dataSize),m_width(width),m_height(height) {}

    ConvertNV12ToJpeg(const ConvertNV12ToJpeg&) = delete;
    ConvertNV12ToJpeg& operator= (const ConvertNV12ToJpeg) = delete;

    cv::Mat Convert(const void* pData) const
    {
        assert(pData != NULL);
        cv::Mat nv12Img(m_height*3/2, m_width, CV_8UC1, (void*)pData);
        cv::Mat jpgImg(m_height, m_width, CV_8UC3);
        cv::cvtColor(nv12Img, jpgImg, cv::COLOR_YUV2BGR_NV12);

        return std::move(jpgImg);
    }
private:
    uint m_width;
    uint m_height;

};

class ConvertBayerToJpeg : public ConvertColorSpace
{
public:
    ConvertBayerToJpeg(uint width, uint height, uint dataSize) : ConvertColorSpace(width, height, dataSize),m_width(width),m_height(height) {}

    ConvertBayerToJpeg(const ConvertBayerToJpeg&) = delete;
    ConvertBayerToJpeg& operator= (const ConvertBayerToJpeg) = delete;

    cv::Mat Convert(const void* pData, const std::string pattern) const
    {
        assert(pData != NULL);
        cv::Mat bayerImg10bit(m_height, m_width, CV_16UC1, (void*)pData);
        cv::Mat bayerImg8bit = cv::Mat::zeros(m_height, m_width, CV_8UC1);
        // raw10 => raw8 => rgb888
        cv::convertScaleAbs(bayerImg10bit, bayerImg8bit, 0.25);
        cv::Mat jpgImg(m_height, m_width, CV_8UC3);

        cv::ColorConversionCodes conversionCode;
        //four bayer pattern: BGGR,GBRG,RGGB,GRBG
        if ("grbg" == pattern)
        {
            conversionCode = cv::COLOR_BayerGRBG2BGR;
        }
        else if ("rggb" == pattern)
        {
            conversionCode= cv::COLOR_BayerRGGB2BGR;
        }
        else if ("gbrg" == pattern)
        {
            conversionCode= cv::COLOR_BayerGBRG2BGR;
        }
        else if ("bggr" == pattern)
        {
            conversionCode= cv::COLOR_BayerBGGR2BGR;
        }
        else
        {
            conversionCode = cv::COLOR_BayerGRBG2BGR;
            printf("default pattern GRBG\n");
        }
        cv::cvtColor(bayerImg8bit, jpgImg, conversionCode);

        return std::move(jpgImg);
    }

private:
    uint m_width;
    uint m_height;
};

// for yuv => jpg
// ./build -d inputdir -f nv12/nv21 --width width --height height [-o outputdir]
// raw => jpg
// raw is only for rawplain16LSB10bit
// ./build -d inputdir -f bayer --width width --height --bayerpattern bggr [-o outputdir]
int main(int argc, const char** argv)
{
    InputCmdParser input(argc, argv);

    auto Usage = [argv](){
        printf("Usage for yuv convert to jpeg:\n");
        printf("%s -d input dir -f nv12/nv21 --width width --height height [-o output dir]\n", argv[0]);
        printf("Usage for bayer convert to jpeg:\n");
        printf("%s -d input dir -f bayer --width width --height height --pattern grbg/rggb/bggr/gbrg [-o output dir]\n", argv[0]);
    };

    if (input.CmdOptionExists("-h") || argc < 2)
    {
        Usage();
        return EXIT_FAILURE;
    }

    Timer timer;

    if (input.CmdOptionExists("-f") &&
        input.CmdOptionExists("--width") && input.CmdOptionExists("--height") &&
        (input.CmdOptionExists("-i") || input.CmdOptionExists("-d")))
    {
        const std::string format        = input.GetCmdOption("-f");
        const std::string widthStr      = input.GetCmdOption("--width");
        const std::string heightStr     = input.GetCmdOption("--height");
        const std::string inputFileName = input.GetCmdOption("-i");
        const std::string inputDir      = input.GetCmdOption("-d");

        uint width  = 0;
        uint height = 0;


        if (!widthStr.empty() && !heightStr.empty())
        {
            width  = std::stoi(widthStr);
            height = std::stoi(heightStr);
        }
        else
        {
            printf("Invalid argument --width %s --height %s\n", widthStr.c_str(), heightStr.c_str());
            return EXIT_FAILURE;
        }

        auto isRawFile = [](const std::string& fileName)->bool {
            std::string suffix = fileName.substr(fileName.find_last_of(".") + 1);
            std::transform(suffix.cbegin(), suffix.cend(), suffix.begin(), [](char c){return std::tolower(c);});
            if (suffix.find("raw") != std::string::npos)
            {
                return true;
            }

            return false;
        };

        auto isYUVFile = [](const std::string& fileName)->bool {
            std::string suffix = fileName.substr(fileName.find_last_of(".") + 1);
            std::transform(suffix.cbegin(), suffix.cend(), suffix.begin(), [](char c){return std::tolower(c);});
            if (suffix.find("nv12") != std::string::npos)
            {
                return true;
            }

            return false;
        };

        struct stat s;
        std::vector<std::string> inputImgVec;
        if ((input.CmdOptionExists("-d")) && (0 == stat(inputDir.c_str(), &s)))
        {
            if (s.st_mode & S_IFDIR)
            {
#ifdef DEBUG
                printf("input dir:%s\n", inputDir.c_str());
#endif
                DIR* dir = opendir(inputDir.c_str());
                struct dirent *ep;
                while ((ep = readdir(dir)))
                {
                    std::string fileName = ep->d_name;
                    if (format == "bayer" && isRawFile(fileName))
                    {
                        fileName = inputDir + "/" + fileName;
#ifdef DEBUG
                        printf("fileName:%s\n", fileName.c_str());
#endif
                        inputImgVec.push_back(fileName);
                    }
                    else if (format == "nv12" && isYUVFile(fileName))
                    {
                        fileName = inputDir + "/" + fileName;
#ifdef DEBUG
                        printf("fileName:%s\n", fileName.c_str());
#endif
                        inputImgVec.push_back(fileName);
                    }
                }
                printf("Get total %zu format %s file(s) in %s\n", inputImgVec.size(), format.c_str(), inputDir.c_str());
            }
            else
            {
                printf("invalid directory\n");
            }
        }

        if (format == "nv12" || format == "nv21")
        {
            ConvertNV12ToJpeg nv12ToJpeg(width, height, width*height*3/2);
            if (input.CmdOptionExists("-d"))
            {
                for (auto imgName : inputImgVec)
                {
                    const ImageData* img = nv12ToJpeg.GetImageData(imgName);
                    cv::Mat jpgImg = nv12ToJpeg.Convert(img->pData);
                    nv12ToJpeg.SaveImage(img->fileName, jpgImg);
                    delete img;
                }
            }
            else if (input.CmdOptionExists("-i"))
            {
                const ImageData* img = nv12ToJpeg.GetImageData(inputFileName);
                cv::Mat jpgImg = nv12ToJpeg.Convert(img->pData);
                nv12ToJpeg.SaveImage(img->fileName, jpgImg);
                delete img;
            }

        }
        else if ((format == "bayer") && input.CmdOptionExists("--pattern"))
        {
            std::string pattern = input.GetCmdOption("--pattern");
            ConvertBayerToJpeg bayerToJpeg(width, height, width*height*2);
            if (input.CmdOptionExists("-d"))
            {

                for (auto imgName : inputImgVec)
                {
                    const ImageData* img = bayerToJpeg.GetImageData(imgName);
                    cv::Mat jpgImg = bayerToJpeg.Convert(img->pData, pattern);
                    bayerToJpeg.SaveImage(img->fileName, jpgImg);
                    delete img;

                }
            }
            else if (input.CmdOptionExists("-i"))
            {
                const ImageData* img = bayerToJpeg.GetImageData(inputFileName);
                cv::Mat jpgImg = bayerToJpeg.Convert(img->pData, pattern);
                bayerToJpeg.SaveImage(img->fileName, jpgImg);
                delete img;
            }
        }
    }
    else
    {
        printf("missing argument\n");
        Usage();
    }

    return 0;
}
