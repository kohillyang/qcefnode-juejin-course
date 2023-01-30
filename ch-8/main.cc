#include "napi.h"
#include <opencv2/opencv.hpp>
class CvMatWrapper {
  public:
    CvMatWrapper(const cv::Mat &image) : mat(image) {}
    static napi_value rows(napi_env env, napi_callback_info info);
    static napi_value cols(napi_env env, napi_callback_info info);
    static napi_value data(napi_env env, napi_callback_info info);
    cv::Mat mat;
};
napi_value CvMatWrapper::rows(napi_env env, napi_callback_info info) {
    CvMatWrapper *p = nullptr;
    napi_status status = napi_ok;
    Napi::CallbackInfo cbInfo(env, info);
    status = napi_unwrap(env, cbInfo.This(), reinterpret_cast<void **>(&p));
    if (status != napi_ok) {
        std::abort();
    }
    return Napi::Number::From(env, p->mat.rows);
}
napi_value CvMatWrapper::cols(napi_env env, napi_callback_info info) {
    CvMatWrapper *p = nullptr;
    napi_status status = napi_ok;
    Napi::CallbackInfo cbInfo(env, info);
    status = napi_unwrap(env, cbInfo.This(), reinterpret_cast<void **>(&p));
    if (status != napi_ok) {
        std::abort();
    }
    return Napi::Number::From(env, p->mat.cols);
}
napi_value CvMatWrapper::data(napi_env env, napi_callback_info info) {
    CvMatWrapper *p = nullptr;
    napi_status status = napi_ok;
    Napi::CallbackInfo cbInfo(env, info);
    status = napi_unwrap(env, cbInfo.This(), reinterpret_cast<void **>(&p));
    if (status != napi_ok) {
        std::abort();
    }
    cv::Mat *bufferMat = new cv::Mat(p->mat);
    size_t data_size = bufferMat->rows * bufferMat->step[0];
    if (data_size > 0) {
        auto data = Napi::ArrayBuffer::New(
          env, bufferMat->data, data_size,
          [](napi_env env, void *data, cv::Mat *finalizeHint) { delete finalizeHint; }, bufferMat);
        return data;
    }
    return cbInfo.Env().Undefined();
}
napi_value Init(napi_env env, napi_callback_info info) {
    napi_status status;
    napi_value target;
    Napi::CallbackInfo cbInfo(env, info);
    status = napi_get_new_target(env, info, &target);
    if (status != napi_ok) {
        std::abort();
    }
    if (target != NULL) {
        CvMatWrapper *cv = nullptr;
        if (cbInfo.Length() == 3 && cbInfo[0].IsNumber() && cbInfo[1].IsNumber()
            && cbInfo[2].IsNumber()) {
            int r = cbInfo[0].As<Napi::Number>();
            int col = cbInfo[1].As<Napi::Number>();
            int type = cbInfo[2].As<Napi::Number>();
            cv::Mat mat(r, col, type);
            cv = new CvMatWrapper(mat);
        } else {
            cv = new CvMatWrapper(cv::Mat());
        }
        status = napi_wrap(
          env, cbInfo.This(), cv,
          [](napi_env env, void *finalize_data, void *finalize_hint) {
              delete reinterpret_cast<CvMatWrapper *>(finalize_data);
          },
          nullptr, nullptr);
        if (status != napi_ok) {
            std::abort();
        }
        napi_property_descriptor properties[] = {
          {"data", 0, 0, CvMatWrapper::data, 0, 0, napi_default, 0},
        };
        status = napi_define_properties(env, cbInfo.This(),
                                        sizeof(properties) / sizeof(properties[0]), properties);
        if (status != napi_ok) {
            std::abort();
        }
        return cbInfo.This();
    }
    return cbInfo.Env().Undefined();
}

NAPI_MODULE_INIT() {
    napi_status status;
    napi_value cls;
    napi_property_descriptor properties[] = {
      {"rows", 0, CvMatWrapper::rows, 0, 0, 0, napi_default, 0},
      {"cols", 0, CvMatWrapper::cols, 0, 0, 0, napi_default, 0},
    };
    status = napi_define_class(env, "CvMat", NAPI_AUTO_LENGTH, Init, nullptr,
                               sizeof(properties) / sizeof(properties[0]), properties, &cls);
    if (status != napi_ok) {
        std::abort();
    }
    return cls;
}