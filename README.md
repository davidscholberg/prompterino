# prompterino

Configurable shell prompt for bash and Windows cmd.

## Install

### Linux

You'll need make and gcc to build this shih.

```bash
git clone https://github.com/davidscholberg/prompterino.git
cd prompterino
ln -s Makefile.gnu Makefile
cp config_sample.h config.h # edit config.h as needed
make install # this installs to ~/.local/bin, so make sure that is on your $PATH
```

Add the following to your .bashrc:

```bash
prompterino_pcmd() {
    PS1="$(prompterino)"
}

PROMPT_COMMAND="prompterino_pcmd"
```

### Windows

You'll need MSVC and clang to build this shih. The following should be run inside an MSVC developer console.

```dosbatch
git clone https://github.com/davidscholberg/prompterino.git
cd prompterino
mklink Makefile Makefile.win

:: edit config.h as needed
copy config_sample.h config.h

:: this installs to %USERPROFILE%\.local\bin, so make sure that is on your %PATH%
nmake install
```

You'll need to install clink in order to be able to customize the cmd shell prompt.

Add the following to `%LOCALAPPDATA%\clink\prompt.lua`:

```lua
local custom_prompt = clink.promptfilter(0)

function custom_prompt:filter()
    return io.popen('prompterino.exe'):read("*a")
end
```
