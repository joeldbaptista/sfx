# `sfx` - simple file explorer

`sfx` is a simple file explorer meant to be used in the command line, 
developed using the suckless development principles.

## Shell prompt prefix

The `:sh` command starts a shell with the environment variable `SFX=1`.
To show `(sfx)` at the start of the prompt in that shell, add this line
to `~/.bashrc`, after the line that sets `PS1`:

```bash
[ -n "$SFX" ] && PS1="(sfx) $PS1"
```

The quotes around `$SFX` are required. Without the quotes, the test is
always true, so every shell shows the prefix.
