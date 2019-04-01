# Janet LuaJIT Bridge


Experimental [Janet](https://janet-lang.org) to [LuaJIT](http://luajit.org) Bridge module.

This module is a minimal Janet bindings to LuaJIT.


## Building 

To build an install LuaJit-2.1 from git locally (linux only).

```
> ./install-luajit-local.sh
```

Alternatively edit `configure.janet` to adapt the paths to your LuaJIT installation. 

To build the Janet native module

```
> janet build
```
To run some tests 

```
> janet test
```

To install the module to your Janet syspath

```
> janet install
```


## Usage

View `test.janet` for basic examples.





