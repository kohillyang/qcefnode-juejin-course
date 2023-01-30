#include <node_api.h>
#include <cstdlib>
NAPI_MODULE_INIT() {
    const char str[] = "hello world";
    napi_value value;
    napi_status status = napi_create_string_utf8(env, str, sizeof(str), &value);
    if (status != decltype(status)::napi_ok) {
        std::abort();
    }
    return value;
}