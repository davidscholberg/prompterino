# prompterino

Configurable shell prompt for bash, windows cmd, and powershell (maybe other shells idk).

## Install

### Linux

You'll need make and gcc to build this shih.

```bash
git clone https://github.com/davidscholberg/prompterino.git
cd prompterino
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

You'll need a mingw gcc compiler to build this shih for windows. Currently the make target for mingw assumes that we're inside wsl with the C drive mounted at `/mnt/c` (cba to make this more general).

```bash
git clone https://github.com/davidscholberg/prompterino.git
cd prompterino
cp config_sample.h config.h # edit config.h as needed
make install-mingw # this installs to %USERPROFILE%\.local\bin, so make sure that is on your %PATH%
```

#### Powershell

Add the following snippet to your `$PROFILE` file:

```ps1
function prompt {
    return ((&"prompterino.exe") -join "`n")
}
```

#### Cmd

You'll need to install clink in order to be able to use the output of prompterino as the cmd shell prompt.

Add the following to `%LOCALAPPDATA%\clink\prompt.lua`:

```lua
local custom_prompt = clink.promptfilter(0)

function custom_prompt:filter()
    return io.popen('prompterino.exe'):read("*a")
end
```
