#include <opencv2/opencv.hpp>
#include <iostream>
#include "reflect.hpp"

class CvMat {
    AU_GADGET
  public:
    CvMat() {}
    CvMat(const cv::Mat &mat_) : mat(mat_) {}
    CvMat(int rows, int cols, int type) : mat(rows, cols, type) {}
    cv::Mat mat;
    AU_INVOKABLE void x1() {}
    AU_INVOKABLE int rows() { return this->mat.rows; }
    AU_INVOKABLE int cols() { return this->mat.cols; }
    AU_INVOKABLE int add(int lhs, int rhs) { return lhs + rhs; }
    AU_INVOKABLE double minus(double lhs, double rhs) { return lhs - rhs; }
};

int main() {
    std::vector<std::string> names = CvMat::methodNames;
    std::cout << CvMat::methodReturnType(0) << std::endl;
    std::cout << CvMat::methodCount() << std::endl;
    for (const auto &x : CvMat::methodParameterTypes(3)) {
        std::cout << QMetaType::typeName(x) << std::endl;
    }
    std::cout << CvMat::className << std::endl;
    CvMat mat;
    std::cout << mat.methodInvoke(3, {QVariant::fromValue<int>(2), QVariant::fromValue<int>(3)})
                   .value<int>()
              << std::endl;
    return 0;
}
#ifndef AURUM_REFLECTION_COMPILER
#include "main.auc"
#endif
