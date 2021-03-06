/* <jhm/status_line.cc>

   Implements <jhm/status_line.h>

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

#include <jhm/status_line.h>

#include <unistd.h>

using namespace std;
using namespace Jhm;

bool Jhm::IsRealTty() {
  static bool is_real_tty = isatty(STDOUT_FILENO);
  return is_real_tty;
}

void TStatusLine::Cleanup() {
  if (IsRealTty()) {
    cout << "\n";
  }
}

TStatusLine::TStatusLine() {
  static bool first = true;
  if (first) {
    first = false;
    return;
  }

  if (IsRealTty()) {
    cout << "\e[2K";
  }
}
TStatusLine::~TStatusLine() {
  if (IsRealTty()) {
    cout << '\r' << std::flush;
  } else {
    cout << '\n';
  }
}
