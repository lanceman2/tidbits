# potato - a web service

It's called potato because I have some potatoes in the front yard that
need to be harvested.  In two weeks it would have been called garlic for
the same reason.

This software package depends on the quickbuild build system which can be
found at: https://github.com/lanceman2/quickbuild

This package provides a simple HTTP web service and is not intended to be
popular.

This package is not intended to be portable and was developed on a Debian
GNU/Linux operating system.  It uses GNU TLS for HTTPS.

## Summary

potato is a simple non-portable, GNU/Linux, C code web server.  Got tried
of not being able to use the cool stuff that is in glibc and the Linux
kernel in a straight forward manner.  No to portability wrappers and all
that extra code.  No portable build systems too.  Yes to performance and
extreme hack-ability.

To see what is here look at the file qb.make.  It is the main development
build file where to see what depends on what C and C header source files.

I've been studying web servers for a few years now and have written C
sockets code professionally in the past.  I know how to C code better than
any other computer language.  I think RUST is the best runner for the
future of what coding will be: ya no garbage collecting, and make the
compiler do as much of the work (debugging and documenting) as possible.
Let's face it compilers are fast now, and interrupted languages will be a
thing of the past.  Maybe compilers will be able to add on the fly
optimizations to executables in the future, leaving interrupted languages
as just part of computer history.  Just a thought...


### Study NodeJS and Nginx

Note: My studying is far from complete, and so a lot of this is just
conjecture.  It's a way to get the ball rolling.  I study by writing my
code and than looking at other peoples code, and then repeat in an
infinite loop.  I find that, that is easier than looking at other
peoples code and than writing your code, because other peoples code is
easier to understand after you have done it yourself.  So I'm a build it
first and then read the directions kind of person.  Life's more fun that
way, for me.

As we write this the biggest server buzz is coming from nodeJS, so I study
it.  They say it's single threaded, but we know that it's not under the
hood.  The two big wins I see from nodejs are (1) asynchronous event
driven callbacks that resemble functional programming, whatever that is,
and (2) using javaScript to configure it.  We see merit in it's simple
design.  A nodeJS server is not necessarily single threaded, the server
programmer just sees a javaScript application programming interface API
that looks that way.  The threading details are hidden from the javaScript
user API.  If you don't buy it than just look up AIO (asynchronous
input/output) on GNU/Linux, or the POSIX AIO API; or look at the V8
javaScript engine and it's thread pool.

Looking at the Nginx source, seems to have lots of portability wrapper
code.  For Linux uses epoll_wait() in a master process putting the socket
accept and existing client read and write events into inter-process shared
memory, with some synchronization method, for the N worker processes to
act on.  In potato we avoid this "portability complexity".


#### server design

The Architecture of potato is simple: a master thread calls epoll_wait()
and does all the accept() calls, and tells thread-pool worker threads to
do reads and writes to the client connected sockets or thread-pool worker
threads just buffers an incomplete client request, and then go on to other
socket read/write event requests.  When configured without a thread-pool a
single thread calls epoll_wait() and then calls the accept(), or reads
the client request, or writes the client in response, or just buffers an
incomplete client request.  The ability to quickly switch the code between
a multi-threaded server and back to single-threaded server is a great for
benchmarking and testing.

We comparing the two extreme server scenarios:

<dt>
    Server with large connection rate with lots of disk access, with
    fast networking
</dt>
<dd>
    In this limit, the multi-threaded server tends to out perform the
    single-threaded server.  Without disk read/write handled by the epoll
    the slow and frequently blocking disk access done in multiple threads,
    keeping computer resources in good use.  Even on a single core
    computer the single-threaded server will not keep computer resources
    in use as well, given the single thread will not do anything while
    waiting for disk access.  Threads are used in POSIX AIO functions,
    such as aio_read(), but POSIX AIO functions may not be an improvement
    since we are using threads in a similar fashion already in potato and
    our threads are also parallelizing the rest of the service, not just
    disk I/O; so what we have may be better than a single threaded server
    with POSIX AIO functions.
</dd>

<dt>
    Server with low connection rate with little of disk access, with fast 
    networking
</dt>
<dd>
    In this limit, the single-threaded server is always the best, using
    less system resources and out performing the multi-threaded server, by
    all measures.  This is the non-CPU-bound case.
</dd>

For server scenarios between these two extreme cases the analysis is not
so simple.  There are lots of things to consider:

For single-threading:

- Pro: Adding threads adds contention via synchronization primitives, like
  pthread_mutex_lock() and pthread_cond_wait() and so on.
- Pro: The kernel socket listening backlog can always queue some waiting
  connections.  

For mult-threading:

- Pro: Many threads can service many requests at one time.
- Pro: Many threads can line-up to do disk I/O.

For mult-process:

- Pro: Separate processes can service many requests at one time with less,
  contention via synchronization primitives as in multi-threaded case.
- Con: processes use more memory and other resources than threads.
- Con: some synchronization contention is just moved to the kernel from
  a threads user-space when compared to mult-threaded.


To run a multiprocess potato server you start the other processes with the
"get listeners" option and it will get the listening sockets from another
running potato server and share the listening sockets, or just add
poTato_fork() after all the poService_create() calls, but before any event
processing calls.  poTato_fork() forks and creates a different epoll file
descriptor for each of the servers in the child process, because it can't
share the one that the parent is using. The Linux kernel handles epoll
events across multiple processes with different kernel event queues with
the same listening socket quite nicely.  Other than the C code that passes
the listening sockets between the potato server processes, very little
additional code was added to make a potato server a multiprocess server.
All the potato server processes can have all the same listening sockets
which are shared via the kernel.  To contrast in order for Nginx to run
multiple processes it makes a master process that polls, calls
epoll_wait(), the listening sockets and uses interprocess shared memory to
pass accept events to the other "worker" processes.  Maybe it's just the
newest Linux kernel, version 3.16 at this writing, being nicer about
sharing listening sockets that potato takes advantage of.  The running
potato processes for a given server installation may or may not be the same
executable, i.e. they may be configured differently.

So, a potato server can have any combination of processes and threads.
Potato processes can be added and removed at any time without service
interruption.

So potato server processes can be added and removed on the fly.  Just
send signal INT to a potato child server process to remove it and it's
children.  To add a process just run the addition with the "get listeners"
option, and it will join the older potato processes in serving.

The V8 javaScript engine makes a blocking call to epoll_wait().  In potato
master thread that calls epoll, so there is centralized blocking in the
event manager that is the master thread, just like in a nodeJS server,
except that with potato we talk about it, and don't pretend it's not
there.  This master thread manages all the service callbacks.  epoll keeps
state in the kernel so that it can be edge triggered and have all it's
service callbacks do nonblocking reads and writes on the socket
connections as the events pop up from epoll.  We make it optimized for
being multi-threaded, but we also let it be a single threaded with almost
no penalty, just some extra thread variable kruft is left in the single
threaded version of a potato server.  All mutex and other synchronization
calls are removed for the single threaded case.  All the mutexes and
synchronization code go away, via CPP macro magic, in the single threaded
version of potato.  Simple tests without disk I/O tend to show that the
single threaded potato server is faster than the multi-threaded potato
server, for many simple server/client low-load test scenarios, as
expected.

We use a thread pool so that we don't spend much time starting and
shutting down threads, as does nodeJS and Nginx (Nginx plans to do).  The
thread pool class is independent of the server code and can be used in
other projects.

TO CONSIDER: I don't see a aio_accept() call, so am I on the right track
by writing my own epoll event loop, or is there a POSIX AIO API interface
to accept() or equivalent.  I see that GNU libc uses a thread pool to
implement parts of AIO API that does not have kernel support.
io_setup(2)


#### Avoiding data copy

http://pl.atyp.us/content/tech/servers.html is a pretty good article.  It
lists data copy at the number one performance-killer.  We don't agree, but
that may be because we avoid data copy without thinking about it.  Context
switches is the biggest problem, and lock contention is a subset of code
that does a context switch, along with thread switching, and system calls
that do user/kernel mode (context) switching.  Lock calls that don't have
contention do not have a big effect on performance, but they do if there
is contention in the lock call.  So putting the locks in a different place
in the code can have a big effect on performance.  Now system V semaphores
incur a system call on every semget() call, even when there is no
contention for the resource, so we use NPTL synchronization primitives
that don't have this flaw.


#### choose C as a configuration language

We choose C code to be the configuration language of potato.  The C
compiler and C preprocessor are the configuration tools we love.  The
potato package in it's simplest form does not build C libraries to build
and install, because that would mean that the installed library was a
particular configuration of potato, seeing as the compiler is the
configurer.  A particular configuration potato manifests its self as it's
C code (and maybe other type) source files.  To save the configuration
you save the source, which if your smart about it should differ from other
configurations by just one or two files.

How do I configure a potato server?  Ans: you write some C code and run
the compiler via make.  If your a C coder it gets no better than that.  If
not, use a different server, or a potato wrapper package.

How can I reconfigure or upgrade a running potato server without shutting
it down?  Compile a your new server, with the old potato server running,
run the new server with the "replace" option.  The new server will signal
the old running potato server and get any usable bound lessoning sockets
from the old server, and than the old potato server will shut down,
leaving the new potato server in it's place.  There will be no service
interruption at all.  The running server reconfiguration is covered while
introducing much more configuration flexibility.

NodeJS uses javaScript for building and configuring, which is it's
greatest merit, if you are a javaScript programmer.  Nginx uses it's own
script language to configure it, which is similar to apache's httpd
method.  Both NodeJS and Nginx require a lot of resource overhead for
configuration.  Compared to potato they become much larger projects
because of how they are configured.  A potato server has it's
configuration compiled into it, making it much smaller in many ways.  Of
course NodeJS has an extreme case of run-time configuration resource
overhead.  Nginx has a lesser case of run-time configuration resource
overhead, i.e. they must be code in Nginx that parses configuration files.
potato comparatively has no run-time configuration resource overhead.
This makes a potato server smaller, with less code.



###


My big reason for writing potato: I'm writing a web service app and I can
code in C faster than in PHP.  I like and know what an operating system is
and I don't need a scripting language getting in my way.

I tried writing an Apache module, and I just couldn't stomach the bloat of
it any longer.  How hard can it be to write your own web server?



## potato Homepage

https://github.com/lanceman2/potato


## Development Notes

### Class Names

<dl>

<dt>
    potato
</dt>
<dd>
    a globel object, a place to put process managment code.  Keeps a list
    of all server objects.  Utilities to fork processes, remove processes,
    and pass listening sockets between processes.
</dd>

<dt>
    server
</dt>
<dd>
    to us a server is a program that waits for requests, server has a
    running event loop calling epoll_wait() that gets service events,
    server manages listers and connections, server manages connections
    that close for various reasons
</dd>

<dt>
    listener
</dt>
<dd>
    listeners are managed by a server, listener is base class for a
    service, listener has a bound socket file descriptor, used to build a
    service, listener has user request callbacks and userData that the user
    calls read() or gnutls_record_recv() in
</dd>

<dt>
    service
</dt>
<dd>
    a listener that has a particular service request callbacks, i.e. it is
    a listener user, used to make for example httpService and
    httpsService, has request callbacks that get passed filled buffers with
    known size
</dd>

<dt>
    connection
</dt>
<dd>
    base class for a connected TCP (with TSL or not) socket, is managed by
    a server, has read, write and cleanup callbacks from a service.  Callbacks
    can be changed in the callbacks
</dd>

<dt>
    threadPool
</dt>
<dd>
    yup it's written.  wikipedia it.
</dd>

<dt>


</dl>


### Random thoughts

Look into using package Memcached.

Agreed, but I add: because flash was propriety it could never evolve into
something worth keeping around.

It's interesting to think into the far future when the web browser
resembles a hypervisor and web apps run in virtual machines that are web
page/tab groups. Web servers become more like package servers.  Such a
browser would be pretty big and complex.  I'll get right on it.

## potato Package Software License

    Copyright (C) 2015  Lance Arsenault

    This package is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This package is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

