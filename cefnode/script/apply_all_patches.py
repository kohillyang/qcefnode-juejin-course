# encoding=utf-8
import os
from subprocess import Popen, PIPE
import sys

git_exe = "git"
def exec_commands(commands, workspace_path):
    process = Popen(
        commands,
        cwd=workspace_path,        
        stdout=sys.stdout,
        stderr=sys.stdout)
    process.communicate()
    return process.returncode

def apply_patch(patch_absolute_path, workspace_path):
    command_args = [git_exe, "am", patch_absolute_path]
    if exec_commands(command_args, workspace_path) != 0:
        print("Failed to apply {}.".format(patch_absolute_path, ))
        sys.exit(-1)

def apply_patches(patches_dir, target_dir):
    for root, _, names in os.walk(patches_dir):
        for name in names:
            patch = os.path.join(root, name)
            if patch.endswith(".patch"):
                apply_patch(patch, target_dir)

if __name__ == "__main__":
    filedir = os.path.abspath(os.path.dirname(__file__))
    apply_patches(os.path.join(filedir, "../patches/node"), os.path.join(filedir, "../node"))
    apply_patches(os.path.join(filedir, "../patches/cef"), os.path.join(filedir, "../../cef"))
    apply_patches(os.path.join(filedir, "../patches/chromium"), os.path.join(filedir, "../../"))