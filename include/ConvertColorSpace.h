#ifndef CONVERTCOLORSPACE_H
#define CONVERTCOLORSPACE_H

#include <opencv2/opencv.hpp>
#include <string>

class ConvertColorSpace {
public:
    ConvertColorSpace(uint width, uint height, uint dataSize);
    ~ConvertColorSpace();

    cv::Mat ConvertNV12ToJpeg(const void* pData);
    cv::Mat ConvertBayerToJpeg(const void* pData, const std::string& pattern);
    void SaveImage(const std::string& fileName, const cv::Mat& img);

private:
    uint m_width;
    uint m_height;
    uint m_dataSize;
    uchar* m_data;
};

#endif // CONVERTCOLORSPACE_H
