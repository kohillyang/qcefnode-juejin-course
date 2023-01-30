# 文件名 webpackmanifest2qrc
# 放在tools目录下
from optparse import OptionParser
import sys
import json

if __name__ == "__main__":
    desc = """将webpack的生成的asset-manifest.json文件转化为Qt和cmake能识别的qrc文件""".format(sys.executable, __file__)
    parser = OptionParser(desc)
    parser.add_option('--input',
                      dest='input',
                      metavar='FILE',
                      help='input webpack build manifest file. [required]')
    parser.add_option('--output',
                      dest='output',
                      metavar='FILE',
                      help='output qt web qrc file path [required]')
    parser.add_option('--prefix',
                      dest='prefix',
                      help='qrc prefix [required]')
    (options, args) = parser.parse_args()
    header = """<RCC>
    <qresource prefix="{}">""".format(options.prefix)
    footer = """    </qresource>
</RCC>"""
    with open(options.output, "wt") as fout:
        print(header, file=fout)
        with open(options.input, "rb") as fin:
            filedict = json.load(fin)  # type: dict
            # dumi 生成的manifest文件中没有files字段
            if "files" in filedict:
                filedict = filedict['files']
            for v in filedict.values():
                item = """        <file>./{}</file>""".format(v)
                print(item, file=fout)
        print(footer, file=fout)
