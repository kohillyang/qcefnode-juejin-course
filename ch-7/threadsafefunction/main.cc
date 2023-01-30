#include "napi.h"
#include <memory>
#include <chrono>
class CallBackContext {
  public:
    CallBackContext(Napi::Env env_, Napi::Function func, int timeout);

    ~CallBackContext();
    static void async_cb(napi_env env, napi_value js_callback, void *context, void *data);

  private:
    napi_ref ref;
    napi_env env;
    napi_threadsafe_function tsfn;
    std::unique_ptr<std::thread> thread;
    CallBackContext(const CallBackContext &) = delete;
    CallBackContext &operator=(const CallBackContext &) = delete;
};
CallBackContext::CallBackContext(Napi::Env env_, Napi::Function func, int timeout) : env(env_) {
    napi_status status = napi_create_reference(env, func, 1, &this->ref);
    if (status != napi_ok) {
        std::abort();
    }
    status = napi_create_threadsafe_function(env, nullptr, nullptr, Napi::String::New(env_, "tsfn"),
                                             0, 1, nullptr, nullptr, this,
                                             CallBackContext::async_cb, &this->tsfn);
    if (status != napi_ok) {
        std::abort();
    }
    this->thread = std::make_unique<std::thread>([this, timeout = timeout]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        napi_status status = napi_call_threadsafe_function(
          this->tsfn, nullptr, napi_threadsafe_function_call_mode::napi_tsfn_nonblocking);
        if (status != napi_ok) {
            std::abort();
        }
    });
}
CallBackContext::~CallBackContext() {
    napi_status status = napi_delete_reference(this->env, this->ref);
    if (status != napi_ok) {
        std::abort();
    }
    this->thread->join();
    status = napi_release_threadsafe_function(
      this->tsfn, napi_threadsafe_function_release_mode::napi_tsfn_abort);
    if (status != napi_ok) {
        std::abort();
    }
}
void CallBackContext::async_cb(napi_env env, napi_value js_callback, void *context, void *data) {
    CallBackContext *self = (CallBackContext *) context;
    napi_status status = napi_ok;
    napi_handle_scope scope;
    status = napi_open_handle_scope(self->env, &scope);
    if (status != napi_ok) {
        std::abort();
    }
    napi_value fn_value;
    status = napi_get_reference_value(self->env, self->ref, &fn_value);
    if (status != napi_ok) {
        std::abort();
    }
    try {
        Napi::Function(self->env, fn_value).Call({});
    } catch (const Napi::Error &e) {
        Napi::Object console = Napi::Env(env).Global().Get("console").As<Napi::Object>();
        Napi::Function errorFn = console.Get("error").As<Napi::Function>();
        errorFn.Call(console, {Napi::Value(env, e.Value())});
    }
    status = napi_close_handle_scope(self->env, scope);
    if (status != napi_ok) {
        std::abort();
    }
    delete self;
}
NAPI_MODULE_INIT() {
    return Napi::Function::New(env, [](const Napi::CallbackInfo &info) {
        if (info.Length() >= 2 && info[0].IsFunction() && info[1].IsNumber()) {
            Napi::Function func = info[0].As<Napi::Function>();
            int timeout = info[1].As<Napi::Number>();
            new CallBackContext(info.Env(), func, timeout);
        }
        return info.Env().Undefined();
    });
}