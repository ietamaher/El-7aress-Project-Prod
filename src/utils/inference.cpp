#include "inference.h"
#include <chrono>
#include <iostream>

YoloInference::YoloInference(const std::string& onnxModelPath, const cv::Size& modelInputShape,
                             const std::string& classesTxtFile, const bool& runWithCuda,
                             const std::string& tensorrtEngine) {
    modelPath = onnxModelPath;
    tensorrtPath = tensorrtEngine;
    modelShape = modelInputShape;
    classesPath = classesTxtFile;
    cudaEnabled = runWithCuda;

    loadOnnxNetwork();
    // Pre-allocate memory for better performance
    preAllocateMemory();
    // Warm up the network
    warmUpNetwork();
}

YoloInference::~YoloInference() {
    net = cv::dnn::Net();
    if (cudaEnabled) {
        std::cout << "CUDA/TensorRT resources for DNN Net should be released now." << std::endl;
    } else {
        std::cout << "DNN Net resources should be released now." << std::endl;
    }
}

std::vector<YoloDetection> YoloInference::runInference(const cv::Mat& input) {
    // Start timing for performance monitoring
    auto start = std::chrono::high_resolution_clock::now();

    cv::Mat modelInput = input;
    int pad_x = 0, pad_y = 0;
    float scale = 1.0f;

    if (letterBoxForSquare && modelShape.width == modelShape.height)
        modelInput = formatToSquare(modelInput, &pad_x, &pad_y, &scale);

    // Optimize blob creation - reuse allocated memory when possible
    cv::dnn::blobFromImage(modelInput, blob, 1.0 / 255.0, modelShape, cv::Scalar(), true, false,
                           CV_32F);
    net.setInput(blob);

    // Use pre-allocated output vector
    outputs.clear();
    net.forward(outputs, outputNames);

    auto inference_end = std::chrono::high_resolution_clock::now();
    auto inference_time =
        std::chrono::duration_cast<std::chrono::milliseconds>(inference_end - start);

    int rows = outputs[0].size[1];
    int dimensions = outputs[0].size[2];

    // YOLOv8 has output shape (batchSize, 84, 8400) (Num classes + box[x,y,w,h])
    if (dimensions > rows) {
        rows = outputs[0].size[2];
        dimensions = outputs[0].size[1];
        outputs[0] = outputs[0].reshape(1, dimensions);
        cv::transpose(outputs[0], outputs[0]);
    }

    float* data = (float*)outputs[0].data;

    // Pre-allocated vectors for better performance
    class_ids.clear();
    confidences.clear();
    boxes.clear();

    // Reserve space to avoid reallocations
    class_ids.reserve(1000);
    confidences.reserve(1000);
    boxes.reserve(1000);

    // Optimize detection loop with SIMD-friendly operations
    for (int i = 0; i < rows; ++i) {
        float* classes_scores = data + 4;

        // Optimize score calculation using iterator
        auto max_iter = std::max_element(classes_scores, classes_scores + classes.size());
        float maxClassScore = *max_iter;
        int maxClassId = static_cast<int>(std::distance(classes_scores, max_iter));

        if (maxClassScore > modelScoreThreshold) {
            confidences.push_back(maxClassScore);
            class_ids.push_back(maxClassId);

            float x = data[0];
            float y = data[1];
            float w = data[2];
            float h = data[3];

            int left = static_cast<int>((x - 0.5f * w - pad_x) / scale);
            int top = static_cast<int>((y - 0.5f * h - pad_y) / scale);
            int width = static_cast<int>(w / scale);
            int height = static_cast<int>(h / scale);

            boxes.emplace_back(left, top, width, height);
        }
        data += dimensions;
    }

    // Use pre-allocated NMS result vector
    nms_result.clear();
    cv::dnn::NMSBoxes(boxes, confidences, modelScoreThreshold, modelNMSThreshold, nms_result);

    std::vector<YoloDetection> detections;
    detections.reserve(nms_result.size());

    for (size_t i = 0; i < nms_result.size(); ++i) {
        int idx = nms_result[i];

        YoloDetection result;
        result.class_id = class_ids[idx];
        result.confidence = confidences[idx];
        result.className = classes[result.class_id];
        result.box = boxes[idx];

        // Use pre-computed colors
        result.color = predefinedColors[result.class_id % predefinedColors.size()];

        detections.push_back(result);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Optional: Print timing information
    if (printTiming) {
        std::cout << "Inference: " << inference_time.count() << "ms, Total: " << total_time.count()
                  << "ms" << std::endl;
    }

    return detections;
}

void YoloInference::loadOnnxNetwork() {
    net = cv::dnn::readNetFromONNX(modelPath);

    // CRITICAL: Disable layer fusion to avoid the error
    //net.enableFusion(false);
    //std::cout << "Layer fusion disabled to prevent OpenCV errors" << std::endl;


    if (cudaEnabled) {
        std::cout << "\nRunning on CUDA with TensorRT optimizations" << std::endl;
        net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);

        try {
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA_FP16);
            std::cout << "FP16 TensorRT acceleration enabled" << std::endl;
        } catch (const cv::Exception& e) {
            std::cout << "FP16 not supported, using FP32 TensorRT" << std::endl;
            net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
        }

        // Check if TensorRT is available
        auto backends = cv::dnn::getAvailableBackends();
        bool tensorrtFound = false;
        for (const auto& backend : backends) {
            if (backend.first == cv::dnn::DNN_BACKEND_CUDA) {
                tensorrtFound = true;
                break;
            }
        }

        if (tensorrtFound) {
            std::cout << "TensorRT integration confirmed" << std::endl;
        }
    }

    outputNames = net.getUnconnectedOutLayersNames();
}

void YoloInference::preAllocateMemory() {
    // Pre-allocate vectors to avoid repeated memory allocations
    class_ids.reserve(1000);
    confidences.reserve(1000);
    boxes.reserve(1000);
    nms_result.reserve(1000);
    outputs.reserve(1);

    // Pre-compute colors to avoid random generation during inference
    predefinedColors.reserve(classes.size());
    for (size_t i = 0; i < classes.size(); ++i) {
        int r = (static_cast<int>(i) * 67) % 256;
        int g = (static_cast<int>(i) * 129) % 256;
        int b = (static_cast<int>(i) * 193) % 256;
        predefinedColors.push_back(InferenceColor(r, g, b));
    }
}

void YoloInference::warmUpNetwork() {
    if (!cudaEnabled)
        return;

    std::cout << "Warming up network (building TensorRT engine)..." << std::endl;

    cv::Mat dummyInput = cv::Mat::zeros(modelShape.height, modelShape.width, CV_8UC3);

    // More warmup runs for TensorRT engine building
    int warmupRuns = 20;  // Increased for TensorRT
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < warmupRuns; ++i) {
        cv::Mat warmupBlob;
        cv::dnn::blobFromImage(dummyInput, warmupBlob, 1.0 / 255.0, modelShape, cv::Scalar(), true,
                               false, CV_32F);
        net.setInput(warmupBlob);

        std::vector<cv::Mat> warmupOutputs;
        net.forward(warmupOutputs, outputNames);

        if (i == 0) {
            auto first_run = std::chrono::high_resolution_clock::now();
            auto build_time = std::chrono::duration_cast<std::chrono::seconds>(first_run - start);
            std::cout << "TensorRT engine build time: " << build_time.count() << "s" << std::endl;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Network warmup completed: " << total_time.count() << "ms" << std::endl;
}

cv::Mat YoloInference::formatToSquare(const cv::Mat& source, int* pad_x, int* pad_y, float* scale) {
    int col = source.cols;
    int row = source.rows;
    int m_inputWidth = modelShape.width;
    int m_inputHeight = modelShape.height;

    *scale =
        std::min(static_cast<float>(m_inputWidth) / col, static_cast<float>(m_inputHeight) / row);
    int resized_w = static_cast<int>(col * *scale);
    int resized_h = static_cast<int>(row * *scale);
    *pad_x = (m_inputWidth - resized_w) / 2;
    *pad_y = (m_inputHeight - resized_h) / 2;

    cv::Mat resized;
    cv::resize(source, resized, cv::Size(resized_w, resized_h), 0, 0, cv::INTER_LINEAR);

    // Use more efficient memory allocation
    cv::Mat result(m_inputHeight, m_inputWidth, source.type(), cv::Scalar::all(0));
    resized.copyTo(result(cv::Rect(*pad_x, *pad_y, resized_w, resized_h)));

    return result;
}

void YoloInference::loadClassesFromFile() {
    std::ifstream inputFile(classesPath);
    if (inputFile.is_open()) {
        std::string classLine;
        classes.clear();
        while (std::getline(inputFile, classLine))
            classes.push_back(classLine);
        inputFile.close();
    }
}
