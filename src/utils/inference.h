#ifndef YOLO_INFERENCE_H
#define YOLO_INFERENCE_H

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <fstream>
#include <vector>
#include <algorithm>

// Simple RGB color struct to replace QColor
struct InferenceColor {
    int r, g, b;
    InferenceColor(int red = 0, int green = 0, int blue = 0) : r(red), g(green), b(blue) {}

    // Convert to cv::Scalar for OpenCV drawing
    cv::Scalar toScalar() const { return cv::Scalar(b, g, r); }
};

struct YoloDetection
{
    int class_id{0};
    std::string className{};
    float confidence{0.0};
    InferenceColor color{};
    cv::Rect box{};
};

class YoloInference
{
public:
    YoloInference(const std::string &onnxModelPath, const cv::Size &modelInputShape,
                  const std::string &classesTxtFile, const bool &runWithCuda,
                  const std::string &tensorrtEngine = "");
    ~YoloInference();

    std::vector<YoloDetection> runInference(const cv::Mat &input);

    // Configuration options
    bool letterBoxForSquare = true;
    float modelScoreThreshold = 0.45f;
    float modelNMSThreshold = 0.50f;
    bool printTiming = false; // Set to true for performance debugging

private:
    void loadOnnxNetwork();
    void loadClassesFromFile();
    void preAllocateMemory();
    void warmUpNetwork();
    cv::Mat formatToSquare(const cv::Mat &source, int *pad_x, int *pad_y, float *scale);

    std::string modelPath{};
    std::string tensorrtPath{};
    std::string classesPath{};
    bool cudaEnabled{};
    bool usingTensorRT{false};

    cv::dnn::Net net;
    cv::Size modelShape{};

    std::vector<std::string> classes{"PERSON", "BICYCLE", "CAR", "MOTORCYCLE", "AIRPLANE", "BUS", "TRAIN", "TRUCK", "BOAT"};
    //, "traffic light", "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse", "sheep", "cow", "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie", "suitcase", "frisbee", "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard", "surfboard", "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana", "apple", "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair", "couch", "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote", "keyboard", "cell phone", "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors", "teddy bear", "hair drier", "toothbrush"};

    // Pre-allocated memory for better performance
    cv::Mat blob;
    std::vector<cv::Mat> outputs;
    std::vector<std::string> outputNames;
    std::vector<int> class_ids;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;
    std::vector<int> nms_result;
    std::vector<InferenceColor> predefinedColors;
};

#endif // YOLO_INFERENCE_H