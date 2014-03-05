/* <stig/sabot/defs.cc>

   Implements <stig/sabot/defs.h>.

   Copyright 2010-2014 Stig LLC

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License. */

#include <stig/sabot/defs.h>

using namespace Stig::Sabot;

TIdxTooBig::TIdxTooBig()
    : runtime_error("idx too big") {}

TUnreachable::TUnreachable()
    : logic_error("unreachable") {}

TNotImplemented::TNotImplemented()
    : runtime_error("not implemented") {}

