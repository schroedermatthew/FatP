/*
FATP_META:
  meta_version: 1
  component: TensorExecution
  file_role: test
  path: components/Tensor/tests/test_TensorExecution_HeaderSelfContained.cpp
  namespace: ""
  layer: Testing
  summary: "Standalone execution facade and serial default smoke test."
  api_stability: in_work
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  related:
    headers:
      - include/fat_p/TensorExecution.h
*/
#include "TensorExecution.h"

int main()
{
    fat_p::Tensor<int> a({1}, 2), b({1}, 3);
    fat_p::TensorExecutionContext context;
    return fat_p::dot(a, b, context)() == 6 && fat_p::matmul(a, b)() == 6 ? 0 : 1;
}
