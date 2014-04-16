/* <orly/expr/while.h>

   TODO

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

#pragma once

#include <memory>

#include <base/no_copy_semantics.h>
#include <orly/expr/thatable_binary.h>

namespace Orly {

  namespace Expr {

    class TWhile
        : public TThatableBinary {
      NO_COPY_SEMANTICS(TWhile);
      public:

      typedef std::shared_ptr<TWhile> TPtr;

      static TPtr New(const TExpr::TPtr &lhs, const TPosRange &pos_range);

      virtual void Accept(const TVisitor &visitor) const;

      virtual Type::TType GetType() const;

      private:

      TWhile(const TExpr::TPtr &lhs, const TPosRange &pos_range);

    };  // TWhile

  }  // Expr

}  // Orly