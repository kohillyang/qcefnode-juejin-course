from subprocess import Popen, PIPE
import os
import sys
import io
import locale
import datetime
import zipfile
import shutil
import argparse

root_dir = os.path.abspath(os.path.dirname(__file__))
cmake_exe = os.path.join(root_dir, "cmake-2.23.2/bin/cmake.exe")
configuration = ["Debug"]


def run_cmake(qt5_dir, source_dir, workspace_path, cfg):
    os.makedirs(workspace_path, exist_ok=True)
    command_args = [cmake_exe, source_dir, "-B./", "-G", "Visual Studio 16 2019", "-Ax64",
                    f"-DQt5_DIR={qt5_dir}"]
    print(" ".join(command_args))
    process = Popen(
        command_args,
        cwd=workspace_path)
    process.communicate()
    assert process.returncode == 0
    process = Popen(
        [cmake_exe, "--build", ".", "--parallel", f"{os.cpu_count()}", "--verbose", "--target ALL_BUILD", "--config",
         cfg, "--", f"/p:CL_MPcount={os.cpu_count()}"],
        cwd=workspace_path,
    )
    process.communicate()
    assert process.returncode == 0


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='编译全部')
    parser.add_argument('--qt5dir', dest='qt5dir', required=True)
    args = parser.parse_args()
    projects = ["ch-3/cef", "ch-3/qwebengine", "ch-4", "ch-5/cef-webchannel", "ch-5/webchannel", "ch-6",
                "ch-7/function", "ch-7/threadsafefunction", "ch-8", "ch-9", "ch-10", "ch-11", "ch-12", "ch-13", "ch-14",
                "ch-16", "ch-18"]
    for prj in projects:
        for cfg in configuration:
            run_cmake(args.qt5dir, os.path.join(root_dir, prj), os.path.join(root_dir, "build", prj), cfg)
