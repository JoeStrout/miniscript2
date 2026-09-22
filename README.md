# MiniScript 2.0

MiniScript 2.0 is complete rewrite of the [MiniScript](https://miniscript.org) language, focused mainly on performance.  It is the cornerstone of the [2026 MiniScript Road Map](https://dev.to/joestrout/miniscript-road-map-for-2026-17mh).  For MiniScript 1.x source code, see [here](https://github.com/JoeStrout/miniscript).

MiniScript 2.0 is developed in C# and transpiled to C++ for production performance. The VM uses 32-bit fixed-width instructions and supports computed-goto dispatch on supported compilers.

See [What's New in MiniScript 2](docs/NewInMS2.md) (also available in [PDF form](docs/NewInMS2.pdf)) for details on what has changed since MiniScript 1.x.

## Building

```bash
tools/build.sh cs         # Build C# only
tools/build.sh transpile  # Generate C++ code from C# code
tools/build.sh cpp        # Build C++ only
tools/build.sh all        # Build everything (C#, transpile, C++)
tools/build.sh test       # Run smoke tests
```

## Related Projects

- [MiniScript 1.x source code](https://github.com/JoeStrout/miniscript)
- [raylib-miniscript](https://github.com/JoeStrout/raylib-miniscript), a complete 2D/3D game engine including physics and fast matrix math, built on MiniScript 2
- [Mini Micro 2](https://github.com/JoeStrout/minimicro2), a rewrite of the [Mini Micro](https://miniscript.org/MiniMicro) neo-retro virtual computer, made with raylib-miniscript

## Sponsor Me!

MiniScript is free, and apart from a small amount of revenue from the [books](https://miniscript.org/books/) and [Unity asset](https://assetstore.unity.com/packages/tools/integration/miniscript-87926), generates no significant income.  Your support is greatly appreciated, and will be used to fund community growth & reward programs like [these](https://miniscript.org/earn.html).

So, [click here to sponsor](https://github.com/sponsors/JoeStrout) -- contributions of any size are greatly appreciated!

## Star History

<a href="https://www.star-history.com/?type=date&legend=top-left&repos=JoeStrout%2Fminiscript2">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=JoeStrout/miniscript2&type=date&theme=dark&legend=top-left&sealed_token=Qf9Yut51tdnKEeaWDrdLjjUh5LPz7HORg9BDLCinGXsHWXIDn5aw_fWq8mdhMTcNwe8iXY6Lajd8OyiTLsHcka_ihGTlqXlqJFEbD1qmKM9JRH7AK8eiug" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=JoeStrout/miniscript2&type=date&legend=top-left&sealed_token=Qf9Yut51tdnKEeaWDrdLjjUh5LPz7HORg9BDLCinGXsHWXIDn5aw_fWq8mdhMTcNwe8iXY6Lajd8OyiTLsHcka_ihGTlqXlqJFEbD1qmKM9JRH7AK8eiug" />
   <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=JoeStrout/miniscript2&type=date&legend=top-left&sealed_token=Qf9Yut51tdnKEeaWDrdLjjUh5LPz7HORg9BDLCinGXsHWXIDn5aw_fWq8mdhMTcNwe8iXY6Lajd8OyiTLsHcka_ihGTlqXlqJFEbD1qmKM9JRH7AK8eiug" />
 </picture>
</a>

Click the ⭐️ at the top to help push this graph up!  Every like helps more people discover MiniScript, and keeps me motivated to push it forward every day!

