import sys
from clang.cindex import CursorKind
from clang.cindex import Index
import os


class MethodMeta:
    name = ""
    argument_types = []  # type: [int]
    return_type = ""
    is_annotated = False
    is_static = False

    def __str__(self):
        return "{}{}({})".format("static " if self.is_static else "", self.name, ",".join(self.argument_types))


class ClassMeta:
    methods = []  # type: [MethodMeta]
    name = ""
    # is_gadget 为True时，表示需要为该类生成代码
    is_gadget = False

    def __str__(self):
        return "Class {}".format(self.name)

    def gen_code(self):
        parameter_types = []
        for m in self.methods:
            types = ", ".join([f"qMetaTypeId<{x}>()" for x in m.argument_types])
            types = "{" + types + "}"
            parameter_types.append(types)
        parameter_types = ", ".join(parameter_types)
        return_types = ", ".join([f"qMetaTypeId<{x.return_type}>()" for x in self.methods])
        invoke_bodies = []
        for method_idx in range(len(self.methods)):
            method = self.methods[method_idx]
            if method.return_type == "void":
                method_args = ", ".join(
                    [f"args[{i}].value<{method.argument_types[i]}>()" for i in range(len(method.argument_types))])
                body = f"        case {method_idx}: this->{method.name}({method_args}); break;"
                invoke_bodies.append(body)
            else:
                method_args = ", ".join(
                    [f"args[{i}].value<{method.argument_types[i]}>()" for i in range(len(method.argument_types))])
                body = f"        case {method_idx}: " \
                       f"r = QVariant::fromValue<{method.return_type}>(this->{method.name}({method_args})); break;"
                invoke_bodies.append(body)
        invoke_body = "\n".join(invoke_bodies)
        method_names = ", ".join([f'"{x.name}"' for x in self.methods])
        template = """
const std::string %(class_name)s::className = "%(class_name)s";
const std::vector<std::string> %(class_name)s::methodNames = {%(method_names)s};
int %(class_name)s::methodCount() {
    return %(method_count)s;
}
int %(class_name)s::methodReturnType(int methodIdx) {
    static int pCount[] = {%(return_types)s};
    if (methodIdx >= %(method_count)s){
        std::abort();
    }
    return pCount[methodIdx];
}
std::vector<int> %(class_name)s::methodParameterTypes(int methodIdx) {
    static std::vector<std::vector<int>> types = {%(parameter_types)s};
    if (methodIdx >= %(method_count)s){
        std::abort();
    }
    return types[methodIdx];
}
QVariant %(class_name)s::methodInvoke(int methodIdx, const std::vector<QVariant> &args) {
    QVariant r;
    switch (methodIdx) {
%(invoke_body)s
        default:
            break;
    }
    return r;
}
""" % ({
            "class_name": self.name,
            "return_types": return_types,
            "method_count": len(self.methods),
            "parameter_types": parameter_types,
            "invoke_body": invoke_body,
            "method_names": method_names
        })
        return template


if __name__ == "__main__":
    METHOD_ANNOTATE = "reflect-invokable"
    AUTOGEN_ANNOTATE = "reflect_autogen"
    inFile = sys.argv[1]
    outFile = sys.argv[2]
    args = sys.argv[3:]
    args.append("-DAURUM_REFLECTION_COMPILER=1")
    index = Index.create()
    translationUnit = index.parse(inFile, args=args)
    diagnostics = [x for x in translationUnit.diagnostics]
    for x in diagnostics:
        print(x.format())
    assert len(diagnostics) == 0
    cursor = translationUnit.cursor
    classes = []
    for c in cursor.get_children():
        kind = c.kind
        location = c.location
        if kind == CursorKind.CLASS_DECL and os.path.samefile(inFile, location.file.name):
            children = [x for x in c.get_children()]
            class_meta = ClassMeta()
            class_meta.name = c.spelling
            is_gadget = False
            for idx in range(len(children)):
                child = children[idx]
                if child.kind == CursorKind.CXX_METHOD:
                    # 类的成员函数
                    # 检查类的函数是否被 AU_INVOKABLE 标记
                    method_meta = MethodMeta()
                    method_children = list(child.get_children())
                    is_annotated = False
                    if len(method_children) >= 1:
                        first_cursor = method_children[0]
                        if first_cursor.kind == CursorKind.ANNOTATE_ATTR and first_cursor.spelling == METHOD_ANNOTATE:
                            is_annotated = True
                        if first_cursor.kind == CursorKind.ANNOTATE_ATTR and first_cursor.spelling == AUTOGEN_ANNOTATE:
                            is_gadget = True
                    method_meta.is_annotated = is_annotated
                    method_meta.is_static = child.is_static_method()
                    method_meta.name = child.spelling
                    method_meta.return_type = child.result_type.spelling
                    argument_types = [x.spelling for x in child.type.argument_types()]
                    method_meta.argument_types = argument_types
                    if method_meta.is_annotated:
                        class_meta.methods.append(method_meta)
            class_meta.is_gadget = is_gadget
            if class_meta.is_gadget:
                classes.append(class_meta)
    with open(outFile, "wt") as f:
        for c in classes:
            print(c.gen_code(), file=f)
