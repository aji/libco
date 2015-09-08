# `libco` &mdash; coroutines for C

*A fun project for doing fun things!* I know that coroutine libraries already
exist for C, but I want to try my hand at writing one to see if I can pull it
off.

## When to use `libco`

`libco` is a **good fit** for the following:

* A document server (e.g. HTTP), shuffling things between disk and the network.
* A graphics rendering master server, sending tasks to multiple slave nodes.
* A middleware API server, responding to requests and possibly making further
  requests.
* A chat server, forwarding messages between multiple connections.
* Shell-like software, managing multiple subprocesses.
* **Anything that waits on data from programs elsewhere and does comparatively
  little processing in between.**

`libco` is a **bad fit** for the following:

* A game server, with discrete time slots for stepping game logic.
* A graphics rendering slave server, receiving tasks and completing them.
* A backend API server, responding to requests by performing some complex
  operation.
* Your CSV parser, which reads from a file and generates a bitmap
* **Anything focused on computation over IO.**
* **Anything focused on operations with no `libco` equivalent.**
* **Anything simple. Use some common sense, friend.**

It may seem like the first "good fit" list covers fewer cases than the "bad
fit" list, and while this may be true in an absolute sense, it's increasingly
the case that solutions are needed to shuffle data between multiple sources
with minimal processing in between, and why shouldn't such an apparently simple
task be actually simple in practice?

## Operations

`libco` provides the following functions. If you can write your blocking code
in terms of these functions, then `libco` is for you:

* Thread management
   * `co_spawn`
* IO
   * `co_read`
   * `co_read_line`
   * `co_write`
   * Filesystem
      * `co_open`
      * `co_close`
   * Sockets
      * `co_connect_tcp`

#### &mdash; Everything below this line is speculation and subject to change &mdash;

## Building/installing

`libco`, in its attempt to remain lightweight, takes the "amalgamation"
approach used by SQLite, here called a "bundle". You can download `libco` as a
header file and a single C source file which you include in your project as you
would any other header or source file. Compiler flags (`-DCO_UCONTEXT`, etc)
determine which backend to use.

## Operations

`libco` provides the following functions:

* Thread management
   * `co_spawn`
* Generators
   * `co_generator`
   * `co_fetch`
   * `co_yield`
* IO
   * `co_read`
   * `co_write`
   * Filesystem
      * `co_open`
      * `co_close`
   * Network
      * `co_connect`
      * `co_bind`
      * `co_accept`
   * Subprocesses
      * `co_fork`
      * `co_exec`
* Timers
   * `co_sleep`
* Multiple producer, single consumer queues
   * `co_mpsc_new`
   * `co_mpsc_put`
   * `co_mpsc_get`

If you can write your software in terms of these primitives and not spend too
much time between calling them, `libco` is an option. Where an operation would
block, `libco` defers the calling thread and puts it in the wait queue. When
the event is ready, `libco` wakes the thread and allows the operation to
continue.

## Backend

`libco` currently only supports `select` and `ucontext` as event and threading
backends, respectively. However, `epoll`, `poll`, and POSIX threads support are
planned for the near future.

## Concepts

With kernel threads/processes, when the process ends up waiting on some
blocking operation such as reading from a file or the network, the kernel marks
the process as waiting and goes to run something else. When the result of that
operation is ready, the kernel takes the next opportunity to wake your process
up. From the view of the process, it seems as if the operation just takes a
long time to run.

The idea in `libco` is similar, but everything is implemented in userspace. A
list of outstanding operations and corresponding waiting threads is kept in a
central event loop. When an event becomes ready, `libco` wakes up the thread so
it can perform the operation unblocked.

IO-driven programs such as servers are often designed using a much more
explicit variant of this same architecture, the difference being that control
for responding to an event passes in and out of a specific handler function.
In this architecture, to read from the network, for example, you register the
socket and a handler function with the event loop and the handler will be
called whenever data is ready. In `libco`, however, you simply use the `libco`
variant `co_read` in a loop. Control is passed to the event loop when blocked,
and passed back to your code when data is ready.

This makes writing complex IO-driven software very ergonomic. Indeed, this
paradigm has been adopted as a built-in language feature in newer languages
like Go.

The drawback, in the case of `libco`, is that the code between blocking
operations cannot run for too long, as `libco` has no preemption mechanism.
However, in most backends `libco` threads are very cheap.
