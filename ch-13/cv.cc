#include "napi.h"
#include <opencv2/opencv.hpp>
#include <QMetaType>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QThreadPool>
#include "uv.h"
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
class UVCallBackContext {
  public:
    UVCallBackContext(Napi::Env env_, const std::function<void(void)> &func_) :
        env(env_), func(func_) {
        napi_status status = napi_ok;
        uv_loop_s *loop;
        status = napi_get_uv_event_loop(this->env, &loop);
        if (status != napi_ok) {
            std::abort();
        }
        this->async = new uv_async_t;
        int r = uv_async_init(loop, this->async, UVCallBackContext::async_cb);
        if (r != 0) {
            std::abort();
        }
        uv_handle_set_data((uv_handle_t *) this->async, this);
    }
    void send() {
        int r = uv_async_send(this->async);
        if (r != 0) {
            std::abort();
        }
    }
    ~UVCallBackContext() {
        uv_close((uv_handle_t *) this->async,
                 +[](uv_handle_t *handle) { delete (uv_async_t *) handle; });
    }
    static void async_cb(uv_async_t *hld) {
        UVCallBackContext *self = (UVCallBackContext *) uv_handle_get_data((uv_handle_t *) hld);
        napi_status status = napi_ok;
        napi_handle_scope scope;
        status = napi_open_handle_scope(self->env, &scope);
        if (status != napi_ok) {
            std::abort();
        }
        Napi::Function fn = Napi::Function::New(self->env, [&](const Napi::CallbackInfo &) {
            try {
                if (self->func != nullptr) {
                    self->func();
                }
            } catch (const Napi::Error &e) {
                Napi::Object console =
                  Napi::Env(self->env).Global().Get("console").As<Napi::Object>();
                Napi::Function errorFn = console.Get("error").As<Napi::Function>();
                errorFn.Call(console, {Napi::Value(self->env, e.Value())});
            }
        });
        fn.Call({});
        status = napi_close_handle_scope(self->env, scope);
        if (status != napi_ok) {
            std::abort();
        }
        delete self;
    }

  private:
    napi_env env;
    uv_async_t *async;
    std::function<void(void)> func;
    UVCallBackContext(const UVCallBackContext &) = delete;
    UVCallBackContext &operator=(const UVCallBackContext &) = delete;
};
class NapiSignalHandler : public QObject {
  private:
    struct MethodMeta {
        std::string signature;
        QMetaMethod::MethodType type;
        std::vector<int> parameterTypes;
        int parameterCount() { return (int) this->parameterTypes.size(); }
        int parameterType(int idx) {
            if (idx >= 0 && idx < (int) this->parameterTypes.size()) {
                return this->parameterTypes[idx];
            }
            return QMetaType::UnknownType;
        }
    };
    class FuncCtx : public std::enable_shared_from_this<FuncCtx> {
      public:
        FuncCtx(Napi::Value func) : env(func.Env()) {
            napi_status status = napi_ok;
            status = napi_create_reference(func.Env(), func, 1, &this->ref);
            if (status != napi_ok) {
                std::abort();
            }
        }
        ~FuncCtx() {
            napi_status status = napi_ok;
            status = napi_delete_reference(this->env, this->ref);
            if (status != napi_ok) {
                std::abort();
            }
        }
        void call(const QVariantList &args) {
            auto cb = [self = this->shared_from_this(), args = args]() {
                auto env = self->env;
                napi_status status = napi_ok;
                napi_handle_scope scope;
                status = napi_open_handle_scope(env, &scope);
                if (status != napi_ok) {
                    std::abort();
                }
                std::unique_ptr<napi_handle_scope, std::function<void(napi_handle_scope *)>>
                  scopeAutoCloser(
                    &scope, [&](napi_handle_scope *) { napi_close_handle_scope(env, scope); });
                for (int i = 0; i < args.size(); ++i) {
                    if (!args[i].isValid()) {
                        std::abort();
                        return;
                    }
                }
                auto fn = [&](const Napi::CallbackInfo &) {
                    std::vector<napi_value> values;
                    Napi::EnvScope envscope(env);
                    for (auto v : args) {
                        int target_type_id = qMetaTypeId<Napi::Value>();
                        if (v.canConvert<Napi::Value>() && v.convert(target_type_id)) {
                            values.push_back(v.value<Napi::Value>());
                        } else if (v.userType() == QMetaType::QVariantList) {
                            auto vaList = v.value<QVariantList>();
                            auto vaListV8 = Napi::Array::New(env, (uint32_t) vaList.size());
                            for (uint32_t i = 0; i < vaListV8.Length(); ++i) {
                                QVariant va = vaList[i];
                                if (va.canConvert<Napi::Value>() && va.convert(target_type_id)) {
                                    vaListV8.Set(i, va.value<Napi::Value>());
                                } else {
                                    std::abort();
                                    return;
                                }
                            }
                            values.push_back(vaListV8);
                        } else {
                            // 注意当QVariant的为Invalid状态时，其typeName返回值为随机地址。
                            if (v.isValid()) {
                                std::abort();
                            } else {
                                // should never reach here.
                                // 进入函数时已做过检查
                                std::abort();
                            }
                            return;
                        }
                    }
                    napi_value result;
                    napi_value call_back = nullptr;
                    status = napi_get_reference_value(env, self->ref, &call_back);
                    if (status != napi_ok) {
                        std::abort();
                    }
                    status = napi_call_function(env, Napi::Env(env).Undefined(), call_back,
                                                values.size(), values.data(), &result);
                    // 回调函数执行过程中可能出现异常
                    if (status == napi_status::napi_pending_exception) {
                        napi_value last_exception;
                        status = napi_get_and_clear_last_exception(env, &last_exception);
                        assert(status == napi_ok);
                        std::string errMsg = Napi::Value(env, last_exception).ToString();
                        Napi::Object console =
                          Napi::Env(env).Global().Get("console").As<Napi::Object>();
                        Napi::Function errorFn = console.Get("error").As<Napi::Function>();
                        errorFn.Call(console, {Napi::Value(env, last_exception)});
                    }
                    if (status != napi_ok) {
                        std::abort();
                    }
                };
                // 创建v8对象需要一个执行上下文，我们可以先创建一个Function，让这个Function执行我们的lambda函数
                // 这样在我们的lambda函数内就有了v8的上下文了
                Napi::Function callback = Napi::Function::New(env, fn);
                napi_value v;
                status =
                  napi_call_function(env, Napi::Env(env).Undefined(), callback, 0, nullptr, &v);
                if (status != napi_ok) {
                    std::abort();
                }
            };
            auto asyncCtx = new UVCallBackContext(env, cb);
            asyncCtx->send();
        }
        bool StrictEquals(const Napi::Value &other) const {
            napi_value funcValue;
            napi_status status = napi_get_reference_value(this->env, this->ref, &funcValue);
            assert(status == napi_ok);
            bool r;
            status = napi_strict_equals(env, funcValue, other, &r);
            assert(status == napi_ok);
            return r;
        };
        FuncCtx(const FuncCtx &) = delete;
        FuncCtx &operator=(const FuncCtx &) = delete;

      private:
        napi_env env;
        napi_ref ref;
    };
    std::vector<MethodMeta> methods;
    std::map<std::string, std::shared_ptr<FuncCtx>> m_cbmap;
    napi_env env;
    std::shared_ptr<QObject> obj;

  public:
    NapiSignalHandler(QObject *object, const napi_env &env_) : env(env_) {
        if (object == nullptr) {
            std::abort();
        }
        auto mo = object->metaObject();
        for (int i = 0; i < mo->methodCount(); ++i) {
            auto method = mo->method(i);
            MethodMeta meta;
            meta.signature = method.methodSignature().toStdString();
            meta.type = method.methodType();
            for (int i = 0; i < method.parameterCount(); i++) {
                meta.parameterTypes.push_back(method.parameterType(i));
            }
            this->methods.push_back(meta);
            if (method.methodType() == QMetaMethod::Signal) {
                QMetaObject::connect(object, i, this, i,
                                     Qt::UniqueConnection | Qt::DirectConnection);
            }
        }
    }
    NapiSignalHandler(std::shared_ptr<QObject> object, const napi_env &env_) : env(env_) {
        this->obj = object;
        if (this->obj == nullptr) {
            std::abort();
        }
        auto mo = object->metaObject();
        for (int i = 0; i < mo->methodCount(); ++i) {
            auto method = mo->method(i);
            MethodMeta meta;
            meta.signature = method.methodSignature().toStdString();
            meta.type = method.methodType();
            for (int i = 0; i < method.parameterCount(); i++) {
                meta.parameterTypes.push_back(method.parameterType(i));
            }
            this->methods.push_back(meta);
            if (method.methodType() == QMetaMethod::Signal) {
                QMetaObject::connect(this->obj.get(), i, this, i,
                                     Qt::UniqueConnection | Qt::QueuedConnection);
            }
        }
    }
    ~NapiSignalHandler() {
        this->m_cbmap.clear();
        this->obj = nullptr;
    }
    int indexOfMethod(const std::string &signature) {
        for (size_t i = 0; i < this->methods.size(); ++i) {
            if (methods[i].signature == signature) {
                return (int) i;
            }
        }
        return -1;
    }
    std::vector<std::string> listSignalList() {
        std::vector<std::string> r;
        for (const auto &x : this->methods) {
            if (x.type == QMetaMethod::Signal) {
                r.push_back(x.signature);
            }
        }
        return r;
    }
    void replaceCallback(const std::string &signal_signature, Napi::Value func) {
        if (func.IsUndefined() || func.IsNull()) {
            this->m_cbmap.erase(signal_signature);
        } else if (func.IsFunction()) {
            auto fn = std::make_shared<FuncCtx>(func);
            this->m_cbmap.erase(signal_signature);
            this->m_cbmap.emplace(signal_signature, std::move(fn));
        }
    }

  private:
    int qt_metacall(QMetaObject::Call _c, int _id_in, void **_a) override {
        int _id = QObject::qt_metacall(_c, _id_in, _a);
        if (_id < 0)
            return _id;
        if (_c == QMetaObject::InvokeMetaMethod) {
            if (_id_in < 0 || _id_in >= (int) this->methods.size()) {
                return _id;
            }
            auto &method = this->methods[_id_in];
            if (method.type != QMetaMethod::Signal) {
                return -1;
            }
            QList<QVariant> args;
            for (int p_idx = 0; p_idx < method.parameterCount(); ++p_idx) {
                int p_type_id = method.parameterType(p_idx);
                QVariant arg;
                if (p_type_id == QMetaType::QVariant) {
                    arg = *reinterpret_cast<QVariant *>(_a[p_idx + 1]);
                } else {
                    arg = QVariant(p_type_id, _a[p_idx + 1]);
                }
                args.append(arg);
            }
            auto func_and_this_obj = this->m_cbmap.find(method.signature);
            if (func_and_this_obj != this->m_cbmap.end()) {
                func_and_this_obj->second->call(args);
            }
            return -1;
        } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
            if (_id < 1)
                *reinterpret_cast<int *>(_a[0]) = -1;
            _id -= 1;
        }
        return _id;
    }

  private:
    NapiSignalHandler &operator=(const NapiSignalHandler &) = delete;
    NapiSignalHandler(const NapiSignalHandler &) = delete;
};
template<typename T, bool isQObject = std::is_base_of<QObject, T>::value> class QGadgetWrapper {
  public:
    QGadgetWrapper(const napi_env &env) {
        this->d = std::make_shared<T>();
        if (isQObject) {
            QObject *p = reinterpret_cast<QObject *>(this->d.get());
            this->signalHandler = std::make_unique<NapiSignalHandler>(p, env);
        }
    }
    QGadgetWrapper(const napi_env &env, std::shared_ptr<T> d_) : d(d_) {
        if (isQObject) {
            QObject *p = reinterpret_cast<QObject *>(this->d.get());
            this->signalHandler = std::make_unique<NapiSignalHandler>(p, env);
        }
    }
    QGadgetWrapper() = delete;
    static napi_value Invoke(napi_env env, napi_callback_info info);
    static napi_value Get(napi_env env, napi_callback_info info);
    static napi_value Set(napi_env env, napi_callback_info info);
    static napi_value Init(napi_env env, napi_callback_info info);
    static std::vector<napi_property_descriptor> staticProperties(Napi::Env env);
    static std::vector<napi_property_descriptor> instanceProperties(Napi::Env env);
    static Napi::Value asNapiValue(Napi::Env env, std::shared_ptr<T> ptr);
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
    static napi_value On(napi_env env, napi_callback_info info_) {
        Napi::CallbackInfo info(env, info_);
        QGadgetWrapper<T> *self = nullptr;
        napi_status status = napi_ok;
        status = napi_unwrap(env, info.This(), reinterpret_cast<void **>(&self));
        if (status != napi_ok) {
            std::abort();
        }
        if (self == nullptr) {
            std::abort();
        }
        if (self->signalHandler != nullptr) {
            auto signalList = self->signalHandler->listSignalList();
            if (info.Length() == 0) {
                Napi::Array arr = Napi::Array::New(info.Env(), (uint32_t) signalList.size());
                for (uint32_t i = 0; i < (uint32_t) signalList.size(); ++i) {
                    arr.Set(i, Napi::String::New(info.Env(), signalList[i]));
                }
                return arr.As<Napi::Value>();
            } else if (info.Length() == 2 && info[0].IsString()
                       && (info[1].IsFunction() || info[1].IsUndefined() || info[1].IsNull())) {
                std::string signalSignature = info[0].As<Napi::String>();
                bool found = false;
                for (const auto &x : signalList) {
                    if (x == signalSignature) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    self->signalHandler->replaceCallback(signalSignature, info[1]);
                } else {
                    Napi::Error::New(info.Env(), "给定的信号不存在。").ThrowAsJavaScriptException();
                }
            } else {
                Napi::Error::New(
                  info.Env(), "on未给定参数时，将返回所有可用的信号列表，当给定参数时，参数长度必"
                              "须为2且第一个参数必须为String，表示要监听的信号")
                  .ThrowAsJavaScriptException();
            }
        }
        return info.Env().Undefined();
    }
    std::unique_ptr<NapiSignalHandler> signalHandler;
    std::shared_ptr<T> d;
};
template<typename T, bool isQObject>
T QGadgetWrapper<T, isQObject>::fromNapiValue(const Napi::Value &v) {
    napi_status status = napi_ok;
    QGadgetWrapper<T> *p1 = nullptr;
    status = napi_unwrap(v.Env(), v, reinterpret_cast<void **>(&p1));
    T *p = p1->d.get();
    if (status != napi_ok) {
        std::abort();
    }
    return *p;
}
template<typename T, bool isQObject>
Napi::Value QGadgetWrapper<T, isQObject>::asNapiValue(Napi::Env env, std::shared_ptr<T> ptr) {
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
    QGadgetWrapper<T> *obj = new QGadgetWrapper(env, ptr);
    status = napi_wrap(
      env, cons_instance, obj,
      [](napi_env env, void *finalize_data, void *finalize_hint) {
          delete reinterpret_cast<QGadgetWrapper<T> *>(finalize_data);
      },
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
    if (isQObject) {
        napi_property_descriptor oneProperty;
        oneProperty.utf8name = "on";
        oneProperty.name = NULL;
        oneProperty.method = &On;
        oneProperty.getter = 0;
        oneProperty.setter = 0;
        oneProperty.attributes = napi_default_method;
        oneProperty.value = 0;
        oneProperty.data = NULL;
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
        QGadgetWrapper<T> *obj = new QGadgetWrapper(env);
        status = napi_wrap(
          env, cbInfo.This(), obj,
          [](napi_env env, void *finalize_data, void *finalize_hint) {
              delete reinterpret_cast<QGadgetWrapper<T> *>(finalize_data);
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
    QGadgetWrapper<T> *p1 = nullptr;
    status = napi_unwrap(env, cbInfo.This(), reinterpret_cast<void **>(&p1));
    if (status != napi_ok) {
        std::abort();
    }
    T *p = p1->d.get();
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
    QGadgetWrapper<T> *obj1 = nullptr;
    status = napi_unwrap(env, cbInfo.This(), reinterpret_cast<void **>(&obj1));
    if (status != napi_ok) {
        std::abort();
    }
    T *obj = obj1->d.get();
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
    QGadgetWrapper<T> *obj1 = nullptr;
    status = napi_unwrap(env, cbInfo.This(), reinterpret_cast<void **>(&obj1));
    if (status != napi_ok) {
        std::abort();
    }
    T *obj = obj1->d.get();
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
    static const int FORMAT_RGBA() { return CV_8UC4; }
    static const int FORMAT_RGB() { return CV_8UC3; }
    static const int FORMAT_GRAY() { return CV_8UC1; }
    Q_PROPERTY(int FORMAT_RGBA READ FORMAT_RGBA)
    Q_PROPERTY(int FORMAT_RGB READ FORMAT_RGB)
    Q_PROPERTY(int FORMAT_GRAY READ FORMAT_GRAY)

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

class AuPromise {
    static const int AU_RPOMISE_STATUS_PENDING = 0;
    static const int AU_RPOMISE_STATUS_RESOLVED = 1;
    static const int AU_RPOMISE_STATUS_REJECTED = 2;
    class AuPromiseBlob {
      public:
        std::mutex m;
        QVariant v;
        std::function<void(bool, QVariant)> fn = nullptr;
        int status = AU_RPOMISE_STATUS_PENDING;
    };
    mutable std::shared_ptr<AuPromiseBlob> blob;

  public:
    AuPromise() { this->blob = std::make_shared<AuPromiseBlob>(); }
    void reject(const QVariant &v) const;
    void resolve(const QVariant &v) const;
    void then(std::function<void(bool, QVariant)> fn) const;
};
Q_DECLARE_METATYPE(AuPromise);
void AuPromise::resolve(const QVariant &v) const {
    std::lock_guard<std::mutex> locker(this->blob->m);
    if (this->blob->status == AU_RPOMISE_STATUS_PENDING) {
        this->blob->v = v;
        this->blob->status = AU_RPOMISE_STATUS_RESOLVED;
        if (this->blob->fn != nullptr) {
            bool rejected = this->blob->status == AU_RPOMISE_STATUS_REJECTED;
            this->blob->fn(rejected, this->blob->v);
        }
    }
}
void AuPromise::reject(const QVariant &v) const {
    std::lock_guard<std::mutex> locker(this->blob->m);
    if (this->blob->status == AU_RPOMISE_STATUS_PENDING) {
        this->blob->v = v;
        this->blob->status = AU_RPOMISE_STATUS_REJECTED;
        if (this->blob->fn != nullptr) {
            bool rejected = this->blob->status == AU_RPOMISE_STATUS_REJECTED;
            this->blob->fn(rejected, this->blob->v);
        }
    }
}
void AuPromise::then(std::function<void(bool, QVariant)> fn) const {
    std::lock_guard<std::mutex> locker(this->blob->m);
    if (this->blob->status != AU_RPOMISE_STATUS_PENDING) {
        bool rejected = this->blob->status == AU_RPOMISE_STATUS_REJECTED;
        this->blob->fn(rejected, this->blob->v);
    } else {
        this->blob->fn = fn;
    }
}

class LambdaQRunnable : public QRunnable {
  public:
    LambdaQRunnable(std::function<void()> fn, bool autoDelete) : fn_(fn) {
        this->setAutoDelete(autoDelete);
    }
    void run() override { this->fn_(); }
    std::function<void()> fn_;
};
class CV : public QObject {
    Q_OBJECT
    Q_CLASSINFO("version", "1")
    static const int INTER_LINEAR() { return cv::INTER_LINEAR; }
    Q_PROPERTY(int INTER_LINEAR READ INTER_LINEAR)
    static const int INTER_CUBIC() { return cv::INTER_CUBIC; }
    Q_PROPERTY(int INTER_CUBIC READ INTER_CUBIC)
  public:
    CV() {}
    ~CV() {}
    Q_INVOKABLE CvMat zeros(int rows, int cols, int type) {
        cv::Mat mat = cv::Mat::zeros(rows, cols, type);
        return CvMat(mat);
    }
    Q_INVOKABLE CvMat ones(int rows, int cols, int type) {
        cv::Mat mat = cv::Mat::zeros(rows, cols, type);
        return CvMat(mat);
    }
    Q_INVOKABLE AuPromise resize(const CvMat &src,
                                 int target_width,
                                 int target_height,
                                 int interpolation) {
        AuPromise promise;
        QThreadPool::globalInstance()->start(new LambdaQRunnable(
          [=]() {
              CvMat r;
              cv::resize(src.mat, r.mat, cv::Size(target_width, target_height), 0.0, 0.0,
                         interpolation);
              promise.resolve(QVariant::fromValue<CvMat>(r));
          },
          true));
        return promise;
    }
    Q_INVOKABLE AuPromise imread(const QString &path) {
        AuPromise promise;
        QThreadPool::globalInstance()->start(new LambdaQRunnable(
          [path = path, promise = promise]() {
              CvMat img = cv::imread(path.toStdString(), cv::IMREAD_UNCHANGED);
              if (img.mat.type() == CV_8UC3) {
                  cv::cvtColor(img.mat, img.mat, cv::COLOR_BGR2RGB);
              }
              if (img.mat.type() == CV_8UC4) {
                  cv::cvtColor(img.mat, img.mat, cv::COLOR_BGRA2RGBA);
              }
              promise.resolve(QVariant::fromValue<CvMat>(img));
          },
          true));
        return promise;
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
template<> Napi::Value convertToNapiValue(const QString &v) {
    return Napi::String::New(Napi::EnvScope::current(), (const char16_t *) v.utf16());
}
template<> QString convertFromNapiValue(const Napi::Value &v) {
    return QString::fromStdU16String(v.As<Napi::String>().Utf16Value());
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
    return QGadgetWrapper<CvMat>::asNapiValue(env, std::make_shared<CvMat>(v));
}
template<> Napi::Value convertToNapiValue(const AuPromise &v) {
    Napi::Env env = Napi::EnvScope::current();
    napi_deferred deferred;
    napi_value promise;
    napi_status status = napi_create_promise(env, &deferred, &promise);
    if (status != napi_status::napi_ok) {
        std::abort();
    }
    AuPromise *data = new AuPromise(v);
    status = napi_wrap(
      env, promise, data,
      [](napi_env, void *finalize_data, void *) {
          delete reinterpret_cast<AuPromise *>(finalize_data);
      },
      nullptr, nullptr);
    if (status != napi_status::napi_ok) {
        std::abort();
    }
    data->then([env = env, deferred = deferred](bool rejected, const QVariant &v) {
        // 在主线程执行 fn
        auto fn = [env = env, v = v, deferred = deferred, rejected = rejected]() {
            QVariant vMutable = v;
            Napi::EnvScope scope(env);
            if (vMutable.canConvert<Napi::Value>()
                && vMutable.convert(qMetaTypeId<Napi::Value>())) {
                if (rejected) {
                } else {
                    napi_status nstatus =
                      napi_resolve_deferred(env, deferred, v.value<Napi::Value>());
                    if (nstatus != napi_ok) {
                        std::abort();
                    }
                }
            } else {
                napi_status nstatus = napi_reject_deferred(
                  env, deferred,
                  Napi::String::New(env, "处理promise时无法将类型转换为Napi::Value"));
                if (nstatus != napi_ok) {
                    std::abort();
                }
            }
        };
        // asyncCtx 在被调用后会被自动析构
        auto asyncCtx = new UVCallBackContext(env, fn);
        asyncCtx->send();
    });
    return Napi::Value(env, promise);
}
void registerConverters() {
    QMetaType::registerConverter<int, Napi::Value>(convertToNapiValue<int>);
    QMetaType::registerConverter<Napi::Value, int>(convertFromNapiValue<int>);
    QMetaType::registerConverter<QString, Napi::Value>(convertToNapiValue<QString>);
    QMetaType::registerConverter<Napi::Value, QString>(convertFromNapiValue<QString>);
    QMetaType::registerConverter<BaseArrayBufferV8, Napi::Value>(
      convertToNapiValue<BaseArrayBufferV8>);
    QMetaType::registerConverter<CvMat, Napi::Value>(convertToNapiValue<CvMat>);
    QMetaType::registerConverter<Napi::Value, CvMat>(convertFromNapiValue<CvMat>);
    QMetaType::registerConverter<AuPromise, Napi::Value>(convertToNapiValue<AuPromise>);
}
NAPI_MODULE_INIT() {
    registerConverters();
    Napi::Object r = Napi::Object::New(env);
    r.Set("CV", QGadgetWrapper<CV>::defineClass(env, "CV"));
    r.Set("CvMat", QGadgetWrapper<CvMat>::defineClass(env, "CvMat"));
    return r;
}

#include "cv.moc"