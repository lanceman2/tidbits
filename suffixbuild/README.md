# This is a copy of the old quickbuild


quickbuild
==========



> a quick, very limited, GNU make based, C code build system


Quickbuild is a C code package kick starter.  It only works on GNU/Linux.
It only builds and installs binary programs, shared libraries, and *.in
files, and .md to .html (maybe others) .  It minimizes the size of the
makefiles that you have to write, to the point that no makefile writing is
required for the default case: making binaries from *.c files.  To make a
shared library requires a one line makefile, for the simplest case.

Quickbuild is a program that generates GNU make files.  Kind of like GNU
autotools but not near as big and robust.  It's not portable.  It's just
for GNU/linux systems (and like).  The quickbuild program installs as a
single ruby executable file.  Running it is a quick way to kick start a
C based software project by writing a very small make file (or none at
all), and your C code files, and that's it.  Running quickbuild creates
project specific make files and supporting build script.  You could start
a project with quickbuild and then add a GNU autotool build to it as
well, after you deem it worthy.  Build system filenames of GNU autotool
files and quickbuild files do not overlap.  For example quickbuild make
files are called qb.make and GNUmakefile, not Makefile.am, Makefile.in, or
Makefile.

Quickbuild lets you write very short make files to make and install
binaries and shared libraries from C code.  It also has utilities for
configured @string@ substitutions.  Think of it as a
quicker/smaller/limited version of GNU autoconf (autotools).

I've written hundreds of little C (and C++) software software projects
that each have very similar fifty (or there about) line GNU make files.
This quickbuild project is a way to put more of this GNU make file code
into a central place where I can share it with many projects and write
much much smaller GNU make files for each project.

Quickbuild gives you a build process of:

  >quickbuild, make, make install.

The bootsstrap (pre-configure) step is a matter of installing quickbuild on
your system or in the package, it's not a very long ruby script.

Quickbuild runs fairly quickly compared to a GNU autotool configure
script.  You can use quickbuild along with GNU autotools on the same
software packages, giving the package builder the option of using either
of the two build tools when they build your code.  Quickbuild makes
hacking the code easier than compared to using GNU autotools.  The
quickbuild make files run much faster and you can use "wildcards" that let
you have make rules that let you add source files without having to edit
the make files or rerun configure.

**Quickbuild** because sometime you tire of having to edit a massive make
file (Makefile.am) in order to add a few C source files to your project.
With the quickbuild build system you're usually not required edit any make
files or rerun configure any time you want to add a C source file to the
package.  In many cases you will not need to edit any make files or rerun
configure, if you add C source files for a library or binary program.
Quickbuild uses wildcard rules to build stuff, unlike GNU automake
(autotools).   That's the big different that makes software development
with quickbuild quick.



## Building and Installing Quickbuild


Just run:

```bash
make install bindir=BINDIR
```
where BINDIR is the directory where you would like to put the quickbuild
executable file.  Or just run 'make' if you don't want to install it.


## Prerequisites


Quickbuild is a **ruby** script.  When I run 'ruby --version' I get
_ruby 2.1.5p273 (2014-11-13) [x86_64-linux-gnu]_

You need a standard GNU C development system with **gcc** and so on.
I'm using a Debian GNU/Linux operating system.



## Examples


See the files in *examples/* for sample software projects that use
quickbuild to configure them.  There's a minimal **hello_world/** software
package in addition to more complex examples.

There is no learning curve to use quickbuild.  The *hello world* example
package requires only one file, *hello.c*.



### You Should Know


that *GNUmakefile* is the filename of the prefered GNU make files file.
See
https://www.gnu.org/software/make/manual/html_node/Makefile-Names.html#Makefile-Names.
Since we use GNU make extensions in the quickbuild generated make files
we name them *GNUmakefile* in order to distinguish them from generic make
files.


### Why?


Mostly I just needed a small quick build tool when I'm too tired to use
GNU autotools.

It builds make files from input files that are kind-of like GNU-autotools
input make file, Makefile.am but with more built-in defaults.  Building
and installing a binary program from C code requires no input file for the
simplest case.   Building and installing a shared library requires just a
one line make input file.

It uses a good scripting language, ruby, which may help it grow.  It
started as a bash script, and then it out grew bash.

I looked at the other build systems
http://en.wikipedia.org/wiki/List_of_build_automation_software.  After
burning about two weeks studying, I'm left thinking that a pile of GNU
make code and a ruby configure script is what I want.

GNU make does the heavy lifting and ruby does the configuring.  It uses
GNU make and not ruby rake; mostly because I know make and not so much
ruby and rake.  GNU make has much more built in than rake.  It appeared to
me that rake was more powerful and generic than GNU make.  GNU make just
seemed more specificilly toned for what I'm doing here and now.  Using
rake would have been a lot more work and code.


### State of the Code (NEWS/TODO)


**2015-Feb-07**  It works as stated above.  Has *make dist* support which
is inclusive by default, making a tarball of all files found after make
distclean is applied, and user opted exclusions are applied.  So some care
is needed to exclude user development kruft files from the tarball.  Needs
C++ compiling support added, which should be very easy.  Who cares about
static libraries.  I don't care about, and can't afford, to make ports to
non-GNU/Linux operating systems.


#### Features

##### Present

C to binary programs, C to shared libraries, .in file @substitution@ which
provides and simple interface to making GNU autoconf configure.ac files
from configure.ac.in files, as well as .c.in to .c and .h.in to .h rules.

Supporting multiple subdirectories is automatic.  Quickbuild sees a
directory with a make file stub in it and does the right thing.  It's just
doing the obvious.

Easy to add *single file to single file* suffix rules, like .md to .html,
which is currently hard coded to run *marked* to make a html file from a
md file.  Where as C programs and C libraries usually have *multiple file
to single file* suffix rules.  So there has got to be at least one line of
make code to tell it how to make compiled C programs and libraries.

Support for individual, source and target, file specific, file type
specific, compiler command line option flags.  Like GNU automake has but
finer control.  For example GNU automake will not let to directly tell the
build system to use particular compiler flags when compiling particular
files for particular target files.  For example the make code

```libfoo.so-init.lo_CPPFLAGS = -DDOG=pug```

might be used to tell quickbuild to use the *-DDOG=pug* C preprocessor
option when compiling *init.c* to make the object file *init.lo* for the
shared library libfoo.so.  There as, the more generic director make
variable

```libfoo.so_CFLAGS = -Wno-format-contains-nul```

might be used to tell quickbuild to use the *-Wno-format-contains-nul*
option any time when the compiler runs to make an object file that is used
to make shared library libfoo.so.  GNU automake currently does not support
anything like the latter.

The make variable *INSTALL* and *NO_INSTALL* are used as selectors for
what files get installed.  By default all installable files are installed.
Only one of *INSTALL* and *NO_INSTALL* may be set. (TODO more here)


##### Future

Support C++ packages... soon I hope.

Should it use GNU libtool?  Looks like lots more work.

Better documentation.

