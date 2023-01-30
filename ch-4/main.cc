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
#include <QFile>
#include <QMimeDatabase>
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
        const std::string url = "http://web/index.html";
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

class MyV8Handler : public CefV8Handler {
  public:
    MyV8Handler() {}

    virtual bool Execute(const CefString &name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList &arguments,
                         CefRefPtr<CefV8Value> &retval,
                         CefString &exception) override {
        if (name == "sayHello") {
            retval = CefV8Value::CreateString("hello world!");
            return true;
        }
        return false;
    }
    IMPLEMENT_REFCOUNTING(MyV8Handler);
};
class ClientAppRenderer : public CefApp, public CefRenderProcessHandler {
    void OnContextCreated(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefV8Context> context) override {
        context->Enter();
        CefRefPtr<CefV8Handler> handler = new MyV8Handler();

        // Create the "myfunc" function.
        CefRefPtr<CefV8Value> func = CefV8Value::CreateFunction("sayHello", handler);
        context->GetGlobal()->SetValue("sayHello", func,
                                       CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_NONE);
        context->Exit();
    }
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() { return this; }

  public:
    ClientAppRenderer() = default;

  private:
    IMPLEMENT_REFCOUNTING(ClientAppRenderer);
    DISALLOW_COPY_AND_ASSIGN(ClientAppRenderer);
};
class QRCResourceHandler : public CefResourceHandler {
  private:
    std::string mime_type_;
    QByteArray data_;
    int offset_;

  public:
    QRCResourceHandler() {
        this->data_ = "";
        this->offset_ = 0;
    }
    bool Open(CefRefPtr<CefRequest> request,
              bool &handle_request,
              CefRefPtr<CefCallback> callback) override {
        CefString method = request->GetMethod();
        std::string url = request->GetURL().ToString();
        QStringList url_split = QString::fromStdString(url).split(":");
        if (url_split.length() == 2) {
            QFile file(QString(":") + url_split[1]);
            if (file.exists() && file.open(QIODevice::ReadOnly)) {
                this->data_ = file.readAll();
                QMimeDatabase db;
                this->mime_type_ =
                  db.mimeTypeForFileNameAndData(file.fileName(), this->data_).name().toStdString();
                handle_request = true;
                return true;
            } else {
                handle_request = true;
                return false;
            }
        }
        handle_request = true;
        return false;
    }
    bool Read(void *data_out,
              int bytes_to_read,
              int &bytes_read,
              CefRefPtr<CefResourceReadCallback> callback) {
        if (offset_ < data_.length()) {
            int bytes_remain = static_cast<int>(data_.length()) - static_cast<int>(offset_);
            int transfer_size = bytes_to_read;
            if (transfer_size > bytes_remain) {
                transfer_size = bytes_remain;
            }
            memcpy(data_out, data_.data() + offset_, transfer_size);
            bytes_read = transfer_size;
            offset_ += transfer_size;
            return true;
        } else {
            bytes_read = 0;
            return false;
        }
    }
    void GetResponseHeaders(CefRefPtr<CefResponse> response,
                            int64 &response_length,
                            CefString &redirectUrl) override {
        response->SetStatus(200);
        response->SetMimeType(this->mime_type_);
        response_length = this->data_.length();
    };
    virtual void Cancel() override {}

  private:
    IMPLEMENT_REFCOUNTING(QRCResourceHandler);
    DISALLOW_COPY_AND_ASSIGN(QRCResourceHandler);
};
class QRCSchemeHandlerFactory : public CefSchemeHandlerFactory {
    CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         const CefString &scheme_name,
                                         CefRefPtr<CefRequest> request) override {
        return new QRCResourceHandler;
    }

  public:
    QRCSchemeHandlerFactory() {}

  private:
    IMPLEMENT_REFCOUNTING(QRCSchemeHandlerFactory);
    DISALLOW_COPY_AND_ASSIGN(QRCSchemeHandlerFactory);
};
int main(int argc, char **argv) {
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
    CefRegisterSchemeHandlerFactory("http", "web", new QRCSchemeHandlerFactory());
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