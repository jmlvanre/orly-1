/* <base/pump.cc>

   Copyright 2010-2014 OrlyAtomics, Inc.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#include <base/pump.h>

#include <sys/epoll.h>
#include <fcntl.h>

#include <util/error.h>
#include <util/io.h>

using namespace Base;
using namespace std;
using namespace Util;

class TPump::TPipe {
  NO_COPY(TPipe);

  public:
  TPipe(TPump *pump, TFd &read, TFd &write) : Pump(pump) {
    assert(&pump);
    assert(&write);
    assert(&read);

    // TODO: The kernel already can do most of this with temporary memory backed files... It just doesn't let us
    //      make a never-blocking stream :(
    // Pipe input from where the user will write to where we will read.
    TFd::Pipe(ReadFd, write);
    SetNonBlocking(ReadFd);

    // Pipe from where we write to where the user will read
    TFd::Pipe(read, WriteFd);
    SetNonBlocking(WriteFd);

    assert(write);
    assert(read);
    assert(ReadFd);
    assert(WriteFd);
  }

  ~TPipe() {
    if (ReadFd.IsOpen()) {
      StopReading();
    }
    if (Writing) {
      StopWriting();
    }
    while (!Blocks.empty()) {
      ReleaseBlock();
    }
  }

  /* Pipes data into the buffer file as needed. Returns true if there will be more processing down the line. */
  bool Service() {
    assert(this);
    static_assert(ReadBufSize > 0, "Read buffer size must be greater than zero otherwise we infinite loop.");

    if (ReadFd.IsOpen()) {
      // If there isn't a buffer to read into, or we are at the end of the current buffer, get a new one.
      if (Blocks.empty() || ReadOffset == ReadBufSize) {
        AcquireBlock();
        ReadOffset = 0;
      }

      // Read up to the end of the current block
      auto read_size = read(ReadFd, Blocks.back() + ReadOffset, ReadBufSize - ReadOffset);
      if (read_size == -1) {  // Error
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          // Do-nothing. We just didn't read data.
        } else {
          ThrowSystemError(errno);
        }
      } else if (read_size == 0) {  // EOF
        StopReading();
      } else {  // Read data
        ReadOffset += read_size;
        StartWriting();

        // NOTE: If we handle being at the end of the buffer / out of block space at the start of reading.
        assert(ReadOffset <= ReadBufSize);
      }
    }

    // Try writing whatever we have available in the current buffer.
    if (Writing) {
      // NOTE: We will write up unitl ReadOffset. It is critical we will read partial blocks or we may get odd
      //      user-visible behavior of waiting until read closes (Which might be a long time after a partial buffer
      //      write) before anything becomes visible to the end reader.
      uint64_t bytes_to_write = 0;
      if (Blocks.empty()) {  // No blocks, nothing to write.
      } else if (Blocks.front() == Blocks.back()) {  // We are in the block with the reader
        bytes_to_write = ReadOffset - WriteOffset;
      } else {  // We're in our own block, write all of it.
        bytes_to_write = ReadBufSize - WriteOffset;
      }

      if (bytes_to_write == 0) {  // There was nothing to write, stop writing.
        StopWriting();
      } else {  // Write what we wanted
        auto write_size = write(WriteFd, Blocks.front() + WriteOffset, bytes_to_write);
        if (write_size == -1) {  // Error
          if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            // Do-nothing. We just didn't write data.
          } else {
            // We are definitely done if we can't write any more (Writing returned an unknown error).
              //TODO: This should purely close out the output pipe. Input pipe should be left unmolested.
              // So that we avoid generating SIGPIPE
            StopReading();
            StopWriting();
            return false;
          }
        } else {
          WriteOffset += write_size;

          assert(WriteOffset <= ReadBufSize);

          // If we're at the same point in the buffer as the read head, release it to simplify things.
          if (Blocks.front() == Blocks.back() && ReadOffset == WriteOffset) {
            ReleaseBlock();
            ReadOffset = 0;
            WriteOffset = 0;
          } else if (WriteOffset == ReadBufSize) { // We're at the end of our own buffer, release it.
            ReleaseBlock();
            WriteOffset = 0;
          }
        }
      }
    }
    return ReadFd.IsOpen() || Writing;
  }

  private:
  void AcquireBlock() { Blocks.push(Pump->BlockPool.Allocate()); }
  void ReleaseBlock() { Pump->BlockPool.Recycle(Pop(Blocks)); }

  void StartWriting() {
    assert(this);
    if (!Writing) {
      Pump->JoinEpoll(WriteFd, EPOLLOUT, this);
      Writing = true;
    }
  }

  void StopWriting() {
    assert(this);
    if (Writing) {
      Pump->PartEpoll(WriteFd);
      Writing = false;
    }
  }

  void StopReading() {
    assert(this);
    assert(ReadFd.IsOpen());
    Pump->PartEpoll(ReadFd);
    ReadFd.Reset();
  }

  /* Pointer to the pump so we can register / deregister as needed, as well as */
  TPump *Pump;

  /* Fd pointing into buffer file. */
  TFd ReadFd;

  /* Fd which should be read from to forward data into buffer file. */
  TFd WriteFd;

  bool Writing = false;

  // NOTE: We explicitly don't force a block to have been entirely written to before we read from it
  // That makes for choppy output if something is spinning off and a block doesn't fill all the way.
  uint64_t ReadOffset = 0;
  uint64_t WriteOffset = 0;
  std::queue<uint8_t *> Blocks;

  friend class TPump;
};

TPump::TPump() : Epoll(epoll_create1(EPOLL_CLOEXEC)) {
  // Add the shutting down fd to the epoll. It'll be the only one with a null pointer associated with it.
  JoinEpoll(Shutdown.GetFd(), EPOLLIN, nullptr);

  // Launch the background thread.
  Background = thread(&TPump::BackgroundMain, this);
}

void TPump::NewPipe(TFd &read, TFd &write) {
  std::lock_guard<mutex> lock(PipeMutex);
  TPipe *pipe = new TPipe(this, read, write);
  Pipes.insert(pipe);

  // TODO: This API should really be
  //   pump->Epoll.AddRead(pipe.get(), pipe->ReadFd, &TPipe::Service);
  //   Where the first item is the pipe, and the second is the identifier, and the last is the service func.
  JoinEpoll(pipe->ReadFd, EPOLLIN, pipe);
}

TPump::~TPump() {
  Shutdown.Push();
  Background.join();

  std::lock_guard<mutex> lock(PipeMutex);
  for (TPipe *pipe : Pipes) {
    delete pipe;
  }
  PipeDied.notify_all();
}

bool TPump::IsIdle() const {
  assert(this);

  //TODO: Make a better guaranteed atomic check for whether or not we are idle
  return Pipes.empty();
}

void TPump::BackgroundMain() {
  assert(this);

  // TODO: Block all signals

  epoll_event events[MaxEventCount];

  bool Stopping = false;

  /* Loop forever, servicing pipes until a stop is requested */
  while (!Stopping) {
    int event_count = epoll_wait(Epoll, events, MaxEventCount, -1);
    if (event_count < 0) {
      if (errno == EINTR) {
        continue;
      } else {
        ThrowSystemError(errno);
      }
    }
    unordered_set<TPipe *> dead_pipes;
    for (int i = 0; i < event_count; ++i) {
      TPipe *pipe = static_cast<TPipe *>(events[i].data.ptr);

      // If pipe is null, this is the exit signal.
      if (pipe) {
        // If we've already been deleted, skip.
        if (Contains(dead_pipes, pipe)) {
          continue;
        }

        if (!pipe->Service()) {
          // The pipe is dead. Remove it from our set of pipes, ping PipeDied.
          dead_pipes.insert(pipe);
          std::lock_guard<mutex> lock(PipeMutex);
          Pipes.erase(pipe);
          PipeDied.notify_all();
          delete pipe;
        }
      } else {
        Stopping = true;
      }
    }
  }
}

void TPump::JoinEpoll(int fd, uint32_t events, TPipe *pipe) {
  assert(this);
  epoll_event event;
  event.events = events;
  event.data.ptr = pipe;
  IfLt0(epoll_ctl(Epoll, EPOLL_CTL_ADD, fd, &event));
}

void TPump::PartEpoll(int fd) {
  assert(this);
  IfLt0(epoll_ctl(Epoll, EPOLL_CTL_DEL, fd, nullptr));
}
