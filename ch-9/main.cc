#include "napi.h"
#include <opencv2/opencv.hpp>
#include <QMetaType>
#include <QMetaMethod>
#include <QMetaProperty>
Q_DECLARE_METATYPE(Napi::Value);
namespace Napi {
class EnvScope {
  public:
    static Napi::Env current();
    EnvScope(napi_env env);
    ~EnvScope();

  private:
    EnvScope(const EnvScope &) = delete;
    EnvScope(const EnvScope &&) = delete;
    EnvScope() = delete;
    EnvScope &operator=(const EnvScope &) = delete;
};
static std::vector<napi_env> envs;
Napi::Env EnvScope::current() {
    if (envs.size() == 0) {
        std::abort();
    }
    return envs.back();
}
EnvScope::EnvScope(napi_env env) {
    envs.push_back(env);
}
EnvScope::~EnvScope() {
    envs.pop_back();
}
}
template<typename T, bool isQObject = std::is_base_of<QObject, T>::value> class QGadgetWrapper {
  public:
    static napi_value Invoke(napi_env env, napi_callback_info info);
    static napi_value Get(napi_env env, napi_callback_info info);
    static napi_value Set(napi_env env, napi_callback_info info);
    static napi_value Init(napi_env env, napi_callback_info info);
    static std::vector<napi_property_descriptor> staticProperties(Napi::Env env);
    static std::vector<napi_property_descriptor> instanceProperties(Napi::Env env);
    static Napi::Value asNapiValue(Napi::Env env, std::unique_ptr<T> ptr);
    static T fromNapiValue(const Napi::Value &v);
    static napi_value defineClass(napi_env env, const char *typeName) {
        napi_status status;
        napi_value cls;
        std::vector<napi_property_descriptor> properties = staticProperties(env);
        status =
          napi_define_class(env, typeName, NAPI_AUTO_LENGTH, Init, nullptr, properties.size(),
                            properties.size() > 0 ? properties.data() : nullptr, &cls);
        if (status != napi_ok) {
            std::abort();
        }
        return cls;
    }
};
template<typename T, bool isQObject>
T QGadgetWrapper<T, isQObject>::fromNapiValue(const Napi::Value &v) {
    napi_status status = napi_ok;
    T *p = nullptr;
    status = napi_unwrap(v.Env(), v, reinterpret_cast<void **>(&p));
    if (status != napi_ok) {
        std::abort();
    }
    return *p;
}
template<typename T, bool isQObject>
Napi::Value QGadgetWrapper<T, isQObject>::asNapiValue(Napi::Env env, std::unique_ptr<T> ptr) {
    napi_status status;
    napi_value cls;
    std::vector<napi_property_descriptor> properties = staticProperties(env);
    status = napi_define_class(
      env, QMetaType::typeName(qMetaTypeId<T>()), NAPI_AUTO_LENGTH,
      [](napi_env env, napi_callback_info info) -> napi_value {
          napi_status status;
          napi_value target;
          Napi::CallbackInfo cbInfo(env, info);
          status = napi_get_new_target(env, info, &target);
          if (status != napi_ok) {
              std::abort();
          }
          if (target != NULL) {
              return cbInfo.This();
          }
          return cbInfo.Env().Undefined();
      },
      nullptr, properties.size(), properties.size() > 0 ? properties.data() : nullptr, &cls);
    if (status != napi_ok) {
        std::abort();
    }
    napi_value cons_instance;
    status = napi_new_instance(env, cls, 0, nullptr, &cons_instance);
    if (status != napi_ok) {
        std::abort();
    }
    status = napi_wrap(
      env, cons_instance, ptr.release(),
      [](napi_env, void *finalize_data, void *) { delete reinterpret_cast<T *>(finalize_data); },
      nullptr, nullptr);
    if (status != napi_ok) {
        std::abort();
    }
    properties = instanceProperties(env);
    if (properties.size() > 0) {
        status = napi_define_properties(env, cons_instance, properties.size(), properties.data());
        if (status != napi_ok) {
            std::abort();
        }
    }
    return Napi::Value(env, cons_instance);
}
template<typename T, bool isQObject>
std::vector<napi_property_descriptor>
QGadgetWrapper<T, isQObject>::staticProperties(Napi::Env env) {
    std::vector<napi_property_descriptor> properties;
    const QMetaObject &meta = T::staticMetaObject;
    for (int pIdx = 0; pIdx < meta.classInfoCount(); ++pIdx) {
        napi_property_descriptor oneProperty;
        QMetaClassInfo p = meta.classInfo(pIdx);
        oneProperty.utf8name = NULL;
        oneProperty.name = Napi::String::New(env, p.name());
        oneProperty.method = 0;
        oneProperty.getter = 0;
        oneProperty.setter = 0;
        oneProperty.attributes = napi_static;
        oneProperty.value = Napi::String::New(env, p.value());
        oneProperty.data = 0;
        properties.push_back(oneProperty);
    }
    for (int mIdx = 0; mIdx < meta.methodCount(); ++mIdx) {
        napi_property_descriptor oneProperty;
        QMetaMethod method = meta.method(mIdx);
        oneProperty.utf8name = NULL;
        oneProperty.name = Napi::String::New(env, method.name().toStdString());
        oneProperty.method = &Invoke;
        oneProperty.getter = 0;
        oneProperty.setter = 0;
        oneProperty.attributes = napi_default_method;
        oneProperty.value = 0;
        oneProperty.data = ((void *) (int64_t) mIdx);
        properties.push_back(oneProperty);
    }
    return properties;
}
template<typename T, bool isQObject>
std::vector<napi_property_descriptor>
QGadgetWrapper<T, isQObject>::instanceProperties(Napi::Env env) {
    std::vector<napi_property_descriptor> properties;
    const QMetaObject &meta = T::staticMetaObject;
    for (int pIdx = 0; pIdx < meta.propertyCount(); ++pIdx) {
        napi_property_descriptor oneProperty;
        QMetaProperty p = meta.property(pIdx);
        oneProperty.utf8name = NULL;
        oneProperty.name = Napi::String::New(env, p.name());
        oneProperty.method = 0;
        oneProperty.getter = p.isReadable() ? Get : 0;
        oneProperty.setter = p.isWritable() ? Set : 0;
        oneProperty.attributes = napi_default;
        oneProperty.value = 0;
        oneProperty.data = ((void *) (int64_t) pIdx);
        properties.push_back(oneProperty);
    }
    return properties;
}
template<typename T, bool isQObject = std::is_base_of<QObject, T>::value>
QVariant invokeMethod(T *obj, const QMetaMethod &method, const QList<QVariant> &args) {
    if (args.size() != method.parameterCount()) {
        std::abort();
    }
    QGenericArgument arguments[10];
    std::vector<QVariant> vaList(args.size());
    for (int i = 0; i < args.size(); ++i) {
        if (method.parameterType(i) == QMetaType::QVariant) {
            QVariant v = args[i];
            vaList[i] = QVariant(QMetaType::QVariant, &v);
            arguments[i] = QGenericArgument(vaList[i].typeName(), vaList[i].constData());
        } else if (method.parameterType(i) == args[i].userType()) {
            arguments[i] = QGenericArgument(args[i].typeName(), args[i].constData());
        } else {
            return QVariant();
        }
    }
    QVariant returnValue;
    bool flag = true;
    if (method.returnType() == QMetaType::Void) {
        if (isQObject) {
            QObject *qobj = reinterpret_cast<QObject *>(obj);
            flag =
              method.invoke(qobj, Qt::ConnectionType::DirectConnection, arguments[0], arguments[1],
                            arguments[2], arguments[3], arguments[4], arguments[5], arguments[6],
                            arguments[7], arguments[8], arguments[9]);
        } else {
            flag = method.invokeOnGadget(obj, arguments[0], arguments[1], arguments[2],
                                         arguments[3], arguments[4], arguments[5], arguments[6],
                                         arguments[7], arguments[8], arguments[9]);
        }
        if (!flag) {
            std::abort();
        }
    } else {
        if (method.returnType() != QMetaType::QVariant) {
            returnValue = QVariant(method.returnType(), 0);
            QGenericReturnArgument returnArgument(method.typeName(), returnValue.data());
            if (isQObject) {
                QObject *qobj = reinterpret_cast<QObject *>(obj);
                flag = method.invoke(qobj, Qt::ConnectionType::DirectConnection, returnArgument,
                                     arguments[0], arguments[1], arguments[2], arguments[3],
                                     arguments[4], arguments[5], arguments[6], arguments[7],
                                     arguments[8], arguments[9]);
            } else {
                flag =
                  method.invokeOnGadget(obj, returnArgument, arguments[0], arguments[1],
                                        arguments[2], arguments[3], arguments[4], arguments[5],
                                        arguments[6], arguments[7], arguments[8], arguments[9]);
            }
            if (!flag) {
                std::abort();
            }
        } else {
            std::abort();
        }
    }
    return returnValue;
}
template<typename T, bool isQObject>
napi_value QGadgetWrapper<T, isQObject>::Init(napi_env env, napi_callback_info info) {
    napi_status status;
    napi_value target;
    Napi::CallbackInfo cbInfo(env, info);
    status = napi_get_new_target(env, info, &target);
    if (status != napi_ok) {
        std::abort();
    }
    if (target != NULL) {
        T *obj = new T();
        status = napi_wrap(
          env, cbInfo.This(), obj,
          [](napi_env env, void *finalize_data, void *finalize_hint) {
              delete reinterpret_cast<T *>(finalize_data);
          },
          nullptr, nullptr);
        if (status != napi_ok) {
            std::abort();
        }
        auto properties = instanceProperties(env);
        if (properties.size() > 0) {
            status =
              napi_define_properties(env, cbInfo.This(), properties.size(), properties.data());
            if (status != napi_ok) {
                std::abort();
            }
        }
        if (status != napi_ok) {
            std::abort();
        }
        return cbInfo.This();
    }
    return cbInfo.Env().Undefined();
}
template<typename T, bool isQObject>
napi_value QGadgetWrapper<T, isQObject>::Invoke(napi_env env, napi_callback_info info) {
    napi_status status = napi_ok;
    Napi::CallbackInfo cbInfo(env, info);
    int methodIndex = (int) (int64_t) cbInfo.Data();
    T *p = nullptr;
    status = napi_unwrap(env, cbInfo.This(), reinterpret_cast<void **>(&p));
    if (status != napi_ok) {
        std::abort();
    }
    const QMetaObject *metaObject = &(p->staticMetaObject);
    if (methodIndex >= 0 && methodIndex < metaObject->methodCount()) {
        QMetaMethod method = metaObject->method(methodIndex);
        if (method.parameterCount() != cbInfo.Length()) {
            Napi::Error::New(env, "参数数量不一致").ThrowAsJavaScriptException();
            return cbInfo.Env().Undefined();
        }
        QList<QVariant> args;
        for (int i = 0; i < method.parameterCount(); ++i) {
            QVariant v = QVariant::fromValue<Napi::Value>(cbInfo[i]);
            if (v.canConvert(method.parameterType(i)) && v.convert(method.parameterType(i))) {
                args.push_back(v);
            } else {
                Napi::Error::New(env, "参数转换失败").ThrowAsJavaScriptException();
                return cbInfo.Env().Undefined();
            }
        }
        Napi::EnvScope scope(env);
        QVariant v = invokeMethod<T>(p, method, args);
        if (!v.isValid()) {
            return cbInfo.Env().Undefined();
        }
        if (v.canConvert(qMetaTypeId<Napi::Value>()) && v.convert(qMetaTypeId<Napi::Value>())) {
            return v.value<Napi::Value>();
        } else {
            Napi::Error::New(env, "结果转换失败").ThrowAsJavaScriptException();
            return cbInfo.Env().Undefined();
        }
    }
    return cbInfo.Env().Undefined();
}
template<typename T, bool isQObject>
napi_value QGadgetWrapper<T, isQObject>::Get(napi_env env, napi_callback_info info) {
    napi_status status = napi_ok;
    Napi::CallbackInfo cbInfo(env, info);
    T *obj = nullptr;
    status = napi_unwrap(env, cbInfo.This(), reinterpret_cast<void **>(&obj));
    if (status != napi_ok) {
        std::abort();
    }
    int pIdx = (int) (int64_t) cbInfo.Data();
    QVariant p;
    if (isQObject) {
        p = obj->staticMetaObject.property(pIdx).read(reinterpret_cast<QObject *>(obj));
    } else {
        p = obj->staticMetaObject.property(pIdx).readOnGadget(obj);
    }
    int napiTypeId = qMetaTypeId<Napi::Value>();
    Napi::EnvScope scope(env);
    if (p.canConvert<Napi::Value>() && p.convert(napiTypeId)) {
        return p.value<Napi::Value>();
    } else {
        Napi::Error::New(env, "属性转换到Napi::Value失败").ThrowAsJavaScriptException();
        return cbInfo.Env().Undefined();
    }
    return cbInfo.Env().Undefined();
}
template<typename T, bool isQObject>
napi_value QGadgetWrapper<T, isQObject>::Set(napi_env env, napi_callback_info info) {
    napi_status status = napi_ok;
    Napi::CallbackInfo cbInfo(env, info);
    T *obj = nullptr;
    status = napi_unwrap(env, cbInfo.This(), reinterpret_cast<void **>(&obj));
    if (status != napi_ok) {
        std::abort();
    }
    if (cbInfo.Length() != 1) {
        Napi::Error::New(env, "参数长度不合法").ThrowAsJavaScriptException();
        return cbInfo.Env().Undefined();
    }
    QVariant v = QVariant::fromValue<Napi::Value>(cbInfo[0]);
    int pIdx = (int) (int64_t) cbInfo.Data();
    QMetaProperty p = obj->staticMetaObject.property(pIdx);
    int typeId = p.userType();
    if (typeId == qMetaTypeId<QVariant>()) {
        Napi::Error::New(env, "不支持目标属性类型为QVariant").ThrowAsJavaScriptException();
        return cbInfo.Env().Undefined();
    }
    if (v.canConvert(typeId) && v.convert(typeId)) {
        bool successFlag = false;
        if (isQObject) {
            successFlag = p.write(reinterpret_cast<QObject *>(obj), v);
        } else {
            successFlag = p.writeOnGadget(obj, v);
        }
        if (!successFlag) {
            Napi::Error::New(env, "写入属性失败").ThrowAsJavaScriptException();
            return cbInfo.Env().Undefined();
        }
    } else {
        Napi::Error::New(env, "类型转换失败").ThrowAsJavaScriptException();
        return cbInfo.Env().Undefined();
    }
    return cbInfo.Env().Undefined();
}
class BaseArrayBufferV8Blob final {
    std::function<void()> fn_;
    void *data_ = nullptr;
    size_t size_ = 0;

  public:
    BaseArrayBufferV8Blob(void *da, size_t s, std::function<void()> fn) :
        fn_(std::move(fn)), data_(da), size_(s) {}
    ~BaseArrayBufferV8Blob() {
        if (this->fn_ != nullptr) {
            this->fn_();
            this->fn_ = nullptr;
        }
    }
    void *data() const { return this->data_; }
    size_t size() const { return this->size_; }
    BaseArrayBufferV8Blob(const BaseArrayBufferV8Blob &) = delete;
    BaseArrayBufferV8Blob &operator=(const BaseArrayBufferV8Blob &) = delete;
};
class BaseArrayBufferV8 final {
    std::shared_ptr<BaseArrayBufferV8Blob> d;

  public:
    BaseArrayBufferV8() : d(nullptr) {}
    BaseArrayBufferV8(void *da, size_t s, std::function<void()> fn) {
        this->d = std::make_shared<BaseArrayBufferV8Blob>(da, s, fn);
    }

    bool isEmpty() const {
        return this->d == nullptr || this->d->data() == nullptr || this->d->size() == 0;
    }
    void *data() const { return this->isEmpty() ? nullptr : this->d->data(); }
    size_t size() const { return this->isEmpty() ? 0 : this->d->size(); }
    ~BaseArrayBufferV8() { this->d = nullptr; }
};
Q_DECLARE_METATYPE(BaseArrayBufferV8);
class CvMat {
    Q_GADGET
    Q_CLASSINFO("version", "1")
  public:
    Q_INVOKABLE int rows() { return this->mat.rows; }
    Q_INVOKABLE int cols() { return this->mat.cols; }
    Q_INVOKABLE int type() { return this->mat.type(); }
    BaseArrayBufferV8 data() {
        cv::Mat *p = new cv::Mat(this->mat);
        return BaseArrayBufferV8(p->data, p->rows * p->step[0], [p]() { delete p; });
    }
    Q_PROPERTY(BaseArrayBufferV8 data READ data)
    CvMat() {}
    CvMat(const cv::Mat &mat_) : mat(mat_) {}
    CvMat(int rows, int cols, int type) : mat(rows, cols, type) {}
    cv::Mat mat;
};
class CV : public QObject {
    Q_OBJECT
    Q_CLASSINFO("version", "1")
    static const int INTER_LINEAR() { return cv::INTER_LINEAR; }
    Q_PROPERTY(int INTER_LINEAR READ INTER_LINEAR)
    static const int INTER_CUBIC() { return cv::INTER_CUBIC; }
    Q_PROPERTY(int INTER_CUBIC READ INTER_CUBIC)
  public:
    Q_INVOKABLE CvMat zeros(int rows, int cols, int type) {
        cv::Mat mat = cv::Mat::zeros(rows, cols, type);
        return CvMat(mat);
    }
    Q_INVOKABLE CvMat ones(int rows, int cols, int type) {
        cv::Mat mat = cv::Mat::zeros(rows, cols, type);
        return CvMat(mat);
    }
    Q_INVOKABLE CvMat resize(const CvMat &src,
                             int target_width,
                             int target_height,
                             int interpolation) {
        if (target_width <= 0 || target_height <= 0 || src.mat.empty()) {
            return CvMat();
        }
        CvMat r;
        cv::resize(src.mat, r.mat, cv::Size(target_width, target_height), 0.0, 0.0, interpolation);
        return r;
    }
};
template<typename T> Napi::Value convertToNapiValue(const T &) {
    static_assert(false);
}

template<typename T> T convertFromNapiValue(const Napi::Value &) {
    static_assert(false);
    return T();
}
template<> Napi::Value convertToNapiValue(const int &v) {
    return Napi::Number::New(Napi::EnvScope::current(), v);
}
template<> int convertFromNapiValue(const Napi::Value &v) {
    return v.As<Napi::Number>();
}
template<> Napi::Value convertToNapiValue(const BaseArrayBufferV8 &v) {
    auto env = Napi::EnvScope::current();
    // 在32位系统上，ArrayBuffer最大支持2GB
    // 在64位系统上，ArrayBuffer最大支持4GB
    if (!v.isEmpty() && v.size() < (size_t) INT32_MAX) {
        // v会以copy的形式捕获，BaseArrayBufferV8复制时只增加引用计数
        auto r = Napi::ArrayBuffer::New(env, v.data(), v.size(), [v](napi_env, void *) {});
        return r;
    } else {
        return Napi::Env(env).Undefined();
    }
}
template<> CvMat convertFromNapiValue(const Napi::Value &v) {
    return QGadgetWrapper<CvMat>::fromNapiValue(v);
}
template<> Napi::Value convertToNapiValue(const CvMat &v) {
    Napi::Env env = Napi::EnvScope::current();
    return QGadgetWrapper<CvMat>::asNapiValue(env, std::make_unique<CvMat>(v));
}
void registerConverters() {
    QMetaType::registerConverter<int, Napi::Value>(convertToNapiValue<int>);
    QMetaType::registerConverter<Napi::Value, int>(convertFromNapiValue<int>);
    QMetaType::registerConverter<BaseArrayBufferV8, Napi::Value>(
      convertToNapiValue<BaseArrayBufferV8>);
    QMetaType::registerConverter<CvMat, Napi::Value>(convertToNapiValue<CvMat>);
    QMetaType::registerConverter<Napi::Value, CvMat>(convertFromNapiValue<CvMat>);
}
NAPI_MODULE_INIT() {
    registerConverters();
    Napi::Object r = Napi::Object::New(env);
    r.Set("CV", QGadgetWrapper<CV>::defineClass(env, "CV"));
    r.Set("CvMat", QGadgetWrapper<CvMat>::defineClass(env, "CvMat"));
    return r;
}

#include "main.moc"