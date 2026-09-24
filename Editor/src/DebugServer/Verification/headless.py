"""How recipes launch the Editor, or a probe linked from it, with no display."""

import shutil


def headless_command(program, *arguments):
    """Wrap a command in Xvfb, rendering on the GPU through VirtualGL when it is installed.

    Xvfb alone gives Mesa's llvmpipe software rasterizer. VirtualGL's EGL back end renders on the
    GPU instead and copies each frame into the Xvfb window, so screenshots and xdotool still work.
    """
    command = ['xvfb-run', '-a']

    if shutil.which('vglrun') is not None:
        command += ['vglrun', '-d', 'egl']

    return command + [str(program), *arguments]
