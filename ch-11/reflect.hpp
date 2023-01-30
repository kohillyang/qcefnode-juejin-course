#include <QVariant>
#include <string>
#include <vector>
#ifdef AURUM_REFLECTION_COMPILER
#define AU_INVOKABLE __attribute__((annotate("reflect-invokable")))
#define AU_AUTOGEN __attribute__((annotate("reflect_autogen")))
#else
#define AU_AUTOGEN
#define AU_INVOKABLE
#endif
#define AU_GADGET                                                                                  \
  public:                                                                                          \
    AU_AUTOGEN static const std::string className;                                                 \
    AU_AUTOGEN static const std::vector<std::string> methodNames;                                    \
    AU_AUTOGEN static int methodCount();                                                           \
    AU_AUTOGEN static int methodReturnType(int methodIdx);                                         \
    AU_AUTOGEN static int methodParameterCount(int methodIdx);                                     \
    AU_AUTOGEN static std::vector<int> methodParameterTypes(int methodIdx);                        \
    AU_AUTOGEN QVariant methodInvoke(int methodIdx, const std::vector<QVariant> &args);            \
                                                                                                   \
  private:
