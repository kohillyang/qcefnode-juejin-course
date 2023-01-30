#include "include/base/cef_macros.h"
#include "include/cef_app.h"
#include "include/cef_sandbox_win.h"
#include <QApplication>
#include <QMainWindow>
#include <QObject>
#include <QWidget>
#include <QDockWidget>
#include <mutex>
#include <condition_variable>
#include <QMetaMethod>
Q_DECLARE_METATYPE(CefRefPtr<CefV8Value>);
static std::mutex browserCountMutex;
static std::condition_variable browserCountCv;
static int browserCount = 0;
class BrowserHandler : public QObject,
                       public CefClient,
                       public CefKeyboardHandler,
                       public CefLifeSpanHandler {
    Q_OBJECT
    void OnAfterCreated(CefRefPtr<CefBrowser> browser_) override {
        if (this->browser == nullptr) {
            this->browser = browser_;
            HWND hwnd_ = this->browser->GetHost()->GetWindowHandle();
            if (this->browser != nullptr) {
                ::SetWindowPos(hwnd_, NULL, 0, 0, this->parentWidth, this->parentHeight,
                               SWP_NOZORDER);
            }
        }
        std::unique_lock<std::mutex> locker(browserCountMutex);
        browserCount += 1;
    }
    void OnBeforeClose(CefRefPtr<CefBrowser> browser_) override {
        std::unique_lock<std::mutex> locker(browserCountMutex);
        browserCount -= 1;
        browserCountCv.notify_one();
    }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }
    bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                       const CefKeyEvent &event,
                       CefEventHandle os_event,
                       bool *is_keyboard_shortcut) override {
        if (event.type == KEYEVENT_RAWKEYDOWN && event.windows_key_code == VK_F9) {
            if (this->browser) {
                CefWindowInfo devtools_info;
                CefRect winRect(0, 0, 600, 400);
                CefBrowserSettings browser_settings;
                devtools_info.SetAsPopup(NULL, "");
                browser->GetHost()->ShowDevTools(devtools_info, nullptr, browser_settings,
                                                 CefPoint());
            }
        }
        return false;
    }

  public:
    void resizeBrowser(int w, int h) {
        if (this->browser != nullptr) {
            HWND hwnd_ = this->browser->GetHost()->GetWindowHandle();
            ::SetWindowPos(hwnd_, NULL, 0, 0, w, h, SWP_NOZORDER);
        }
        this->parentWidth = w;
        this->parentHeight = h;
    }

  public:
    BrowserHandler() = default;

  private:
    CefRefPtr<CefBrowser> browser;
    std::atomic<int> parentWidth = 0;
    std::atomic<int> parentHeight = 0;
    IMPLEMENT_REFCOUNTING(BrowserHandler);
    DISALLOW_COPY_AND_ASSIGN(BrowserHandler);
};
class QCefWidget : public QWidget {
    Q_OBJECT
  public:
    QCefWidget(QWidget *parent = nullptr) : QWidget(parent) {
        this->setContentsMargins(0, 0, 0, 0);
        CefBrowserSettings browser_settings;
        CefWindowInfo window_info;
        CefRect winRect(0, 0, this->width(), this->height());
        window_info.SetAsChild((HWND) this->winId(), winRect);
        const std::string url = "http://www.baidu.com";
        this->client = new BrowserHandler;
        CefBrowserHost::CreateBrowser(window_info, this->client, CefString(url), browser_settings,
                                      nullptr, CefRequestContext::GetGlobalContext());
    }
    void resizeEvent(QResizeEvent *e) override {
        if (this->client) {
            this->client->resizeBrowser(this->width(), this->height());
            return QWidget::resizeEvent(e);
        }
    }

  private:
    CefRefPtr<BrowserHandler> client;
};

class CefSignalTask : public CefTask {
  public:
    CefSignalTask(QVariantList args_, CefRefPtr<CefV8Context> ctx_, CefRefPtr<CefV8Value> func_) :
        args(args_), ctx(ctx_), func(func_) {}
    QVariantList args;
    CefRefPtr<CefV8Context> ctx;
    CefRefPtr<CefV8Value> func;
    virtual void Execute() override {
        CefV8ValueList arguments;
        for (int i = 0; i < args.size(); ++i) {
            if (args[i].canConvert(qMetaTypeId<CefRefPtr<CefV8Value>>())
                && args[i].convert(qMetaTypeId<CefRefPtr<CefV8Value>>())) {
                arguments.push_back(args[i].value<CefRefPtr<CefV8Value>>());
            } else {
                return;
            }
        }
        this->func->ExecuteFunctionWithContext(this->ctx, ctx->GetGlobal(), arguments);
    }

  private:
    IMPLEMENT_REFCOUNTING(CefSignalTask);
    DISALLOW_COPY_AND_ASSIGN(CefSignalTask);
};
class SignalHandler : public QObject {
  public:
    SignalHandler(QObject *object, CefRefPtr<CefV8Context> ctx_) : obj(object), ctx(ctx_) {
        auto mo = object->metaObject();
        for (int i = 0; i < mo->methodCount(); ++i) {
            auto method = mo->method(i);
            if (method.methodType() == QMetaMethod::Signal) {
                QMetaObject::connect(object, i, this, i,
                                     Qt::UniqueConnection | Qt::DirectConnection);
            }
        }
    }
    int qt_metacall(QMetaObject::Call _c, int _id_in, void **_a) override {
        if (!CefCurrentlyOn(CefThreadId::TID_RENDERER)) {
            std::abort();
        }
        int _id = QObject::qt_metacall(_c, _id_in, _a);
        if (_id < 0)
            return _id;
        if (_c == QMetaObject::InvokeMetaMethod) {
            if (_id_in < 0 || _id_in >= (int) this->obj->metaObject()->methodCount()) {
                return _id;
            }
            auto method = this->obj->metaObject()->method(_id_in);
            if (method.methodType() != QMetaMethod::Signal) {
                return -1;
            }
            QList<QVariant> args;
            int mc = method.parameterCount();
            std::string n = method.methodSignature().toStdString();
            for (int p_idx = 0; p_idx < mc; ++p_idx) {
                int p_type_id = method.parameterType(p_idx);
                QVariant arg;
                if (p_type_id == QMetaType::QVariant) {
                    arg = *reinterpret_cast<QVariant *>(_a[p_idx + 1]);
                } else {
                    arg = QVariant(p_type_id, _a[p_idx + 1]);
                }
                args.append(arg);
            }
            auto func = this->functions.find(method.methodSignature().toStdString());
            if (func != this->functions.end()) {
                CefRefPtr<CefSignalTask> task = new CefSignalTask(args, this->ctx, func->second);
                CefPostTask(TID_RENDERER, task);
            }
            return -1;
        } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
            if (_id < 1)
                *reinterpret_cast<int *>(_a[0]) = -1;
            _id -= 1;
        }
        return _id;
    }
    void on(const std::string &signal, CefRefPtr<CefV8Value> func) {
        if (func->IsFunction()) {
            this->functions.emplace(signal, func);
        }
    }

  private:
    std::map<std::string, CefRefPtr<CefV8Value>> functions;
    CefRefPtr<CefV8Context> ctx;
    QObject *obj;
    SignalHandler(const SignalHandler &) = delete;
    SignalHandler &operator=(const SignalHandler &) = delete;
};
class ServerObject : public QObject {
    Q_OBJECT
  public:
    Q_INVOKABLE int addOne(const int &p) { return p + 1; }
    Q_INVOKABLE void triggerSignal() { return this->testSignal(); }
  Q_SIGNALS:
    void testSignal();
};
class MyV8Handler : public CefV8Handler {
  public:
    MyV8Handler(QObject *obj_, CefRefPtr<CefV8Context> ctx) : obj(obj_) {
        this->signalHandler = std::make_unique<SignalHandler>(obj_, ctx);
    }
    CefRefPtr<CefV8Value> GetProxyObject() {
        CefRefPtr<CefV8Value> r = CefV8Value::CreateObject(nullptr, nullptr);
        int mc = this->obj->metaObject()->methodCount();
        for (int i = 0; i < mc; ++i) {
            QMetaMethod method = this->obj->metaObject()->method(i);
            if (method.methodType() == QMetaMethod::MethodType::Method
                && method.access() == QMetaMethod::Access::Public) {
                auto func =
                  CefV8Value::CreateFunction(method.methodSignature().toStdString(), this);
                r->SetValue(method.name().toStdString(), func,
                            CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_NONE);
            }
        }
        r->SetValue("on", CefV8Value::CreateFunction("on", this),
                    CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_NONE);
        return r;
    }
    virtual bool Execute(const CefString &name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList &arguments,
                         CefRefPtr<CefV8Value> &retval,
                         CefString &exception) override {
        auto mo = this->obj->metaObject();
        if (name == "on" && arguments.size() == 2 && arguments[1]->IsFunction()
            && arguments[0]->IsString()) {
            this->signalHandler->on(arguments[0]->GetStringValue().ToString(), arguments[1]);
            return true;
        }
        int methodIndex = mo->indexOfMethod(name.ToString().c_str());
        if (methodIndex == -1) {
            exception = "找不到给定的函数";
            return true;
        }
        QMetaMethod method = mo->method(methodIndex);
        if (method.parameterCount() != arguments.size()) {
            exception = "参数数量不一致";
            return true;
        }
        std::vector<QGenericArgument> args;
        args.resize(10);
        for (int i = 0; i < arguments.size(); ++i) {
            QVariant v = QVariant::fromValue<CefRefPtr<CefV8Value>>(arguments.at(i));
            int tgtTypeID = method.parameterType(i);
            if (v.canConvert(tgtTypeID) && v.convert(tgtTypeID)) {
                args[i] = QGenericArgument(v.typeName(), v.data());
            } else {
                exception = "参数转换失败";
                return true;
            }
        }
        if (method.returnType() == QMetaType::Void) {
            method.invoke(this->obj, Qt::ConnectionType::DirectConnection, args[0], args[1],
                          args[2], args[3], args[4], args[5], args[6], args[7], args[8], args[9]);
            return true;
        } else {
            QVariant returnValue(method.returnType(), 0);
            QGenericReturnArgument returnArgument(method.typeName(), returnValue.data());
            method.invoke(this->obj, Qt::ConnectionType::DirectConnection, returnArgument, args[0],
                          args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8],
                          args[9]);
            if (returnValue.canConvert(qMetaTypeId<CefRefPtr<CefV8Value>>())
                && returnValue.convert(qMetaTypeId<CefRefPtr<CefV8Value>>())) {
                retval = returnValue.value<CefRefPtr<CefV8Value>>();
                return true;
            } else {
                exception = "结果转换失败";
                return true;
            }
        }
        return false;
    }
    QObject *obj;
    std::unique_ptr<SignalHandler> signalHandler;
    IMPLEMENT_REFCOUNTING(MyV8Handler);
};
class ClientAppRenderer : public CefApp, public CefRenderProcessHandler {
  public:
    ClientAppRenderer() { this->obj = new ServerObject; }
    ~ClientAppRenderer() { delete this->obj; }
    void OnContextCreated(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefV8Context> context) override {
        context->Enter();
        CefRefPtr<MyV8Handler> handler = new MyV8Handler(this->obj, context);
        context->GetGlobal()->SetValue("proxy", handler->GetProxyObject(),
                                       CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_NONE);
        context->Exit();
    }
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() { return this; }

  private:
    QObject *obj = nullptr;
    IMPLEMENT_REFCOUNTING(ClientAppRenderer);
    DISALLOW_COPY_AND_ASSIGN(ClientAppRenderer);
};

int main(int argc, char **argv) {
    QMetaType::registerConverter<int, CefRefPtr<CefV8Value>>(
      [](const int &v) { return CefV8Value::CreateInt(v); });
    QMetaType::registerConverter<CefRefPtr<CefV8Value>, int>(
      [](const CefRefPtr<CefV8Value> &v) { return v->GetIntValue(); });

    // 所有文件/v8/QString均以utf-8编码
    // 将控制台编码设为utf-8使得向控制台打印消息时不用经过编码转换
    SetConsoleOutputCP(CP_UTF8);
    CefRefPtr<CefCommandLine> command_line = CefCommandLine::CreateCommandLine();
    command_line->InitFromString(::GetCommandLineW());
    const char *kProcessType = "type";
    if (command_line->HasSwitch(kProcessType)) {
        const std::string &process_type = command_line->GetSwitchValue(kProcessType);
        CefMainArgs main_args(GetModuleHandleA(NULL));
        if (process_type == "renderer") {
            QApplication app(argc, argv);
            app.setQuitOnLastWindowClosed(false);
            CefRefPtr<CefApp> cefapp = new ClientAppRenderer();
            return CefExecuteProcess(main_args, cefapp, NULL);
        }
        return CefExecuteProcess(main_args, nullptr, NULL);
    }
    // 子进程将自动继承该job，避免主进程退出后子进程不会自动退出
    HANDLE hjob = CreateJobObject(NULL, NULL);
    std::unique_ptr<HANDLE, void (*)(HANDLE *)> jobCloser(&hjob, [](HANDLE *hld) {
        if (*hld != NULL) {
            BOOL success = ::CloseHandle(*hld);
            assert(success == TRUE);
        }
    });
    if (hjob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobli = {0};
        jobli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(hjob, JobObjectExtendedLimitInformation, &jobli, sizeof(jobli));
        AssignProcessToJobObject(hjob, GetCurrentProcess());
    }
    CefMainArgs main_args(GetModuleHandle(NULL));
    CefSettings settings;
    settings.log_severity = LOGSEVERITY_DISABLE;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = true;
    CefInitialize(main_args, settings, nullptr, nullptr);
    QApplication app(argc, argv);
    int r = 0;
    {
        QCefWidget w;
        w.resize(640, 480);
        w.show();
        r = app.exec();
    }
    {
        std::unique_lock<std::mutex> locker(browserCountMutex);
        if (!browserCountCv.wait_for(locker, std::chrono::seconds(2),
                                     [&]() { return browserCount == 0; })) {
            // timeout
            jobCloser = nullptr;
        };
    }
    CefQuitMessageLoop();
    CefShutdown();


    return r;
}
#include "main.moc"