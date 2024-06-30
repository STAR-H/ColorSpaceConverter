#include "ConvertColorSpace.h"
#include <opencv2/imgcodecs.hpp>
#include <cassert>

ConvertColorSpace::ConvertColorSpace(uint width, uint height, uint dataSize)
    : m_width(width), m_height(height), m_dataSize(dataSize), m_data(nullptr) {
    assert(m_dataSize > 0);
    m_data = new uchar[m_dataSize];
}

ConvertColorSpace::~ConvertColorSpace() {
    delete[] m_data;
}

cv::Mat ConvertColorSpace::ConvertNV12ToJpeg(const void* pData) {
    cv::Mat yuvImg(m_height + m_height / 2, m_width, CV_8UC1, const_cast<void*>(pData));
    cv::Mat bgrImg;
    cv::cvtColor(yuvImg, bgrImg, cv::COLOR_YUV2BGR_NV12);
    return bgrImg;
}

cv::Mat ConvertColorSpace::ConvertBayerToJpeg(const void* pData, const std::string& pattern) {
    cv::Mat bayerImg(m_height, m_width, CV_8UC1, const_cast<void*>(pData));
    cv::Mat bgrImg;
    if (pattern == "rggb") {
        cv::cvtColor(bayerImg, bgrImg, cv::COLOR_BayerBG2BGR);
    } else if (pattern == "grbg") {
        cv::cvtColor(bayerImg, bgrImg, cv::COLOR_BayerGB2BGR);
    } else if (pattern == "gbrg") {
        cv::cvtColor(bayerImg, bgrImg, cv::COLOR_BayerGR2BGR);
    } else if (pattern == "bggr") {
        cv::cvtColor(bayerImg, bgrImg, cv::COLOR_BayerRG2BGR);
    }
    return bgrImg;
}

void ConvertColorSpace::SaveImage(const std::string& fileName, const cv::Mat& img) {
    cv::imwrite(fileName, img);
}
