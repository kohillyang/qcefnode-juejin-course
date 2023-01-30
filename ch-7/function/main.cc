#include "napi.h"
#include "uv.h"
#include <memory>
#include <chrono>
class CallBackContext {
  public:
    CallBackContext(Napi::Env env_, Napi::Function func, int timeout);

    ~CallBackContext();
    static void async_cb(uv_async_t *hld);

  private:
    napi_ref ref;
    napi_env env;
    uv_async_t *async;
    std::unique_ptr<std::thread> thread;
    CallBackContext(const CallBackContext &) = delete;
    CallBackContext &operator=(const CallBackContext &) = delete;
};
CallBackContext::CallBackContext(Napi::Env env_, Napi::Function func, int timeout) : env(env_) {
    napi_status status = napi_create_reference(env, func, 1, &this->ref);
    if (status != napi_ok) {
        std::abort();
    }
    uv_loop_s *loop;
    status = napi_get_uv_event_loop(this->env, &loop);
    if (status != napi_ok) {
        std::abort();
    }
    this->async = new uv_async_t;
    int r = uv_async_init(loop, this->async, CallBackContext::async_cb);
    if (r != 0) {
        std::abort();
    }
    uv_handle_set_data((uv_handle_t *) this->async, this);
    this->thread = std::make_unique<std::thread>([this, timeout = timeout]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
        int r = uv_async_send(this->async);
        if (r != 0) {
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
    uv_close((uv_handle_t *) this->async,
             +[](uv_handle_t *handle) { delete (uv_async_t *) handle; });
}
void CallBackContext::async_cb(uv_async_t *hld) {
    CallBackContext *self = (CallBackContext *) uv_handle_get_data((uv_handle_t *) hld);
    napi_status status = napi_ok;
    napi_handle_scope scope;
    status = napi_open_handle_scope(self->env, &scope);
    if (status != napi_ok) {
        std::abort();
    }
    Napi::Function fn = Napi::Function::New(self->env, [&](const Napi::CallbackInfo &) {
        napi_value fn_value;
        napi_status status = napi_get_reference_value(self->env, self->ref, &fn_value);
        if (status != napi_ok) {
            std::abort();
        }
        try {
            Napi::Function(self->env, fn_value).Call({});
        } catch (const Napi::Error &e) {
            Napi::Object console = Napi::Env(self->env).Global().Get("console").As<Napi::Object>();
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