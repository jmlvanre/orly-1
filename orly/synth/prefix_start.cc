/* <orly/synth/prefix_start.cc>

   Implements <orly/synth/prefix_start.h>.

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

#include <orly/synth/prefix_start.h>

#include <base/no_default_case.h>
#include <orly/expr/start.h>
#include <orly/synth/context.h>
#include <orly/synth/get_pos_range.h>
#include <orly/synth/new_expr.h>
#include <orly/synth/startable_expr.h>

using namespace Orly;
using namespace Orly::Synth;

TPrefixStart::TPrefixStart(const TExprFactory *expr_factory, const Package::Syntax::TPrefixStart *prefix_start)
    : PrefixStart(Base::AssertTrue(prefix_start)),
      Expr(Base::AssertTrue(expr_factory)->NewExpr(PrefixStart->GetExpr())),
      StartableExpr(expr_factory->StartableExpr) {
  if (!StartableExpr) {
    GetContext().AddError(GetPosRange(PrefixStart), "'start' outside of 'reduce', 'collated_by'");
  }
}

Expr::TExpr::TPtr TPrefixStart::Build() const {
  assert(this);
  auto start = Expr::TStart::New(Expr->Build(), GetPosRange(PrefixStart));
  auto startable = StartableExpr->GetStartableSymbol();
  if (!startable->TryGetStart()) {
    startable->SetStart(start);
  } else {
    GetContext().AddError(GetPosRange(PrefixStart), "'reduce' and 'collated_by' must have exactly one start");
  }
  return start;
}

void TPrefixStart::ForEachInnerScope(const std::function<void (TScope *)> &cb) {
  assert(this);
  assert(&cb);
  assert(cb);
  Expr->ForEachInnerScope(cb);
}

void TPrefixStart::ForEachRef(const std::function<void (TAnyRef &)> &cb) {
  assert(this);
  assert(&cb);
  assert(cb);
  Expr->ForEachRef(cb);
}